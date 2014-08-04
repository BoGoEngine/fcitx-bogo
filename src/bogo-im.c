#include <fcitx/fcitx.h>
#include <fcitx/ime.h>
#include <fcitx/instance.h>
#include <fcitx-utils/utf8.h>
#include <iconv.h>
#include <Python.h>

#include "config.h"

#define LOGGING

#ifdef LOGGING
    #define LOG(fmt...) printf(fmt)
#else
    #define LOG(fmt...)
#endif

#ifdef LIBICONV_SECOND_ARGUMENT_IS_CONST
typedef const char* IconvStr;
#else
typedef char* IconvStr;
#endif

PyObject *bogo_process_sequence_func;


/*
 * fcitx-bogo is a shared library that will be dynamically linked
 * by the fctix daemon. When it does, it looks for the 'ime' symbol,
 * which it will use to find the setup and teardown functions
 * for the library.
 */

static void* FcitxBogoSetup(FcitxInstance* instance);
static void FcitxBogoTeardown(void* arg);

FCITX_EXPORT_API
FcitxIMClass ime = {
    FcitxBogoSetup,
    FcitxBogoTeardown
};

FCITX_EXPORT_API
int ABI_VERSION = FCITX_ABI_VERSION; 


typedef struct {
    // The handle to talk to fcitx
    FcitxInstance *fcitx;
    iconv_t conv;
    char *raw_string;
    int raw_string_len;
    
    char *previous_result;
} Bogo;

int FcitxUnikeyUcs4ToUtf8(Bogo *self, const unsigned int c, char buf[UTF8_MAX_LENGTH + 1]);


// Public interface functions
static boolean BogoOnInit(Bogo *self);
static INPUT_RETURN_VALUE BogoOnKeyEvent(void* arg, FcitxKeySym sym, unsigned int state);
static void BogoOnReset(Bogo *self);
static void BogoOnSave(Bogo *self);
static void BogoOnConfig(Bogo *self);


void* FcitxBogoSetup(FcitxInstance* instance)
{
    Bogo *bogo = malloc(sizeof(Bogo));
    memset(bogo, 0, sizeof(Bogo));
    
    bogo->fcitx = instance;

    FcitxIMIFace iface;
    memset(&iface, 0, sizeof(FcitxIMIFace));

    iface.Init = BogoOnInit;
    iface.ResetIM = BogoOnReset;
    iface.DoInput = BogoOnKeyEvent;
    iface.ReloadConfig = BogoOnConfig;
    iface.Save = BogoOnSave;

    FcitxInstanceRegisterIMv2(
        instance,   // fcitx instance
        bogo,       // IME object
        "bogo",     // unique name
        "Bogo",     // human-readable name
        "bogo",     // icon name
        iface,      // interface for the IME object
        1,          // priority
        "vi"        // language
    );
    
    union {
        short s;
        unsigned char b[2];
    } endian;
    endian.s = 0x1234;
    if (endian.b[0] == 0x12)
        bogo->conv = iconv_open("utf-8", "ucs-4be");
    else
        bogo->conv = iconv_open("utf-8", "ucs-4le");
    
    Py_SetProgramName(L"fcitx-bogo");  /* optional but recommended */
    Py_Initialize();
    PyRun_SimpleString("from time import time,ctime\n"
                       "print('Today is', ctime(time()))\n");
    
    PyObject *moduleName, *bogoModule;
    moduleName = PyUnicode_FromString("bogo");
    bogoModule = PyImport_Import(moduleName);
    Py_DECREF(moduleName);

    bogo_process_sequence_func = PyObject_GetAttrString(bogoModule, "process_sequence");

    return bogo;
}

void FcitxBogoTeardown(void* arg)
{
    LOG("Destroyed\n");
    Py_Finalize();
}

boolean BogoOnInit(Bogo *self)
{
    LOG("Init\n");
    
    BogoOnReset(self);
    
    return true;
}

INPUT_RETURN_VALUE BogoOnKeyEvent(void* arg, FcitxKeySym sym, unsigned int state)
{
    Bogo *self = (Bogo *) arg;
    
    if (sym >= FcitxKey_space && sym <=FcitxKey_asciitilde) {
        // Convert the keysym to UTF8
        char *sym_utf8 = malloc(UTF8_MAX_LENGTH + 1);
        memset(sym_utf8, 0, UTF8_MAX_LENGTH + 1);
        FcitxUnikeyUcs4ToUtf8(self, sym, sym_utf8);
        
        // FIXME: Realloc can fail
        if (strlen(self->raw_string) + strlen(sym_utf8) > self->raw_string_len) {
            realloc(self->raw_string, self->raw_string_len * 2);
        }
        strcat(self->raw_string, sym_utf8);
        
        PyObject *args, *pyResult;
        
        args = Py_BuildValue("(s)", self->raw_string);
        pyResult = PyObject_CallObject(bogo_process_sequence_func, args);
        Py_DECREF(args);

        char *result = strdup(PyUnicode_AsUTF8(pyResult));
        Py_DECREF(pyResult);
        
        // Find the number of different chars
        int i = 0;
        int diff_offset = -1;
        for (; i < strlen(self->previous_result); ++i) {
            if (self->previous_result[i] != result[i]) {
                break;
            }
        }
        diff_offset = i;
        
        // The number of backspaces to send is the number of UTF8 chars
        // at the end of previous_result that differ from result.
        int num_backspace = 0;
        num_backspace = fcitx_utf8_strlen(self->previous_result + diff_offset);
        
        LOG("num_backspace: %d\n", num_backspace);
        for (i = 0; i < num_backspace; ++i) {
            FcitxInstanceForwardKey(
                        self->fcitx,
                        FcitxInstanceGetCurrentIC(self->fcitx),
                        FCITX_PRESS_KEY,
                        FcitxKey_BackSpace,
                        0);
            
            FcitxInstanceForwardKey(
                        self->fcitx,
                        FcitxInstanceGetCurrentIC(self->fcitx),
                        FCITX_RELEASE_KEY,
                        FcitxKey_BackSpace,
                        0);
        }
        
//        FcitxInstanceDeleteSurroundingText(
//                    self->fcitx,
//                    FcitxInstanceGetCurrentIC(self->fcitx),
//                    -num_backspace,
//                    num_backspace);

        LOG("%d %d\n", strlen(self->previous_result), diff_offset);
        LOG("string to commit: %s\n", result + diff_offset);
        
        FcitxInstanceCommitString(
                    self->fcitx,
                    FcitxInstanceGetCurrentIC(self->fcitx),
                    result + diff_offset);
        
        LOG("%s %s\n", sym_utf8, result);
        
        free(self->previous_result);
        self->previous_result = result;
        
        return IRV_TO_PROCESS;
    }
    
    return IRV_FLAG_FORWARD_KEY;
}

void BogoOnReset(Bogo *self)
{
    LOG("Reset\n");
    self->previous_result = malloc(1);
    self->previous_result[0] = 0;
    self->raw_string = malloc(512);
    self->raw_string[0] = 0;
    self->raw_string_len = 512;
}

void BogoOnSave(Bogo *self)
{
    LOG("Saved\n");
}

void BogoOnConfig(Bogo *self)
{
    LOG("Reload config\n");
}

int FcitxUnikeyUcs4ToUtf8(Bogo *self, const unsigned int c, char buf[UTF8_MAX_LENGTH + 1])
{
    unsigned int str[2];
    str[0] = c;
    str[1] = 0;

    size_t ucslen = 1;
    size_t len = UTF8_MAX_LENGTH;
    len *= sizeof(char);
    ucslen *= sizeof(unsigned int);
    char* p = buf;
    IconvStr src = (IconvStr) str;
    iconv(self->conv, &src, &ucslen, &p, &len);
    return (UTF8_MAX_LENGTH - len) / sizeof(char);
}

