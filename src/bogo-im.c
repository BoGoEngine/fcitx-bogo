#include <fcitx/fcitx.h>
#include <fcitx/ime.h>
#include <fcitx/instance.h>
#include <fcitx-utils/utf8.h>
#include <iconv.h>
#include <Python.h>
#include <time.h>

#include "config.h"

#define LOGGING
#define INITIAL_STRING_LEN 128

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
static INPUT_RETURN_VALUE BogoOnKeyPress(Bogo *self, FcitxKeySym sym, unsigned int state);
static void BogoOnReset(Bogo *self);
static void BogoOnSave(Bogo *self);
static void BogoOnConfig(Bogo *self);

boolean SupportSurroundingText(Bogo *self);
INPUT_RETURN_VALUE CommitString(Bogo *self, char *str);


void* FcitxBogoSetup(FcitxInstance* instance)
{
    Bogo *bogo = malloc(sizeof(Bogo));
    memset(bogo, 0, sizeof(Bogo));
    
    bogo->fcitx = instance;

    FcitxIMIFace iface;
    memset(&iface, 0, sizeof(FcitxIMIFace));

    iface.Init = BogoOnInit;
    iface.ResetIM = BogoOnReset;
    iface.DoInput = BogoOnKeyPress;
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

    // Load the bogo-python engine
    Py_SetProgramName(L"fcitx-bogo");
    Py_Initialize();

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


void BogoInitialize(Bogo *self) {
    self->previous_result = malloc(1);
    self->previous_result[0] = 0;
    self->raw_string = malloc(INITIAL_STRING_LEN);
    self->raw_string[0] = 0;
    self->raw_string_len = INITIAL_STRING_LEN;
}


boolean BogoOnInit(Bogo *self)
{
    LOG("Init\n");
    BogoInitialize(self);
    return true;
}


void BogoOnReset(Bogo *self)
{
    LOG("Reset\n");
    if (self->previous_result) {
        free(self->previous_result);
    }
    
    if (self->raw_string) {
        free(self->raw_string);
    }
    
    BogoInitialize(self);
}


boolean CanProcess(FcitxKeySym sym, unsigned int state)
{
    if (state & (FcitxKeyState_Ctrl |
                 FcitxKeyState_Alt |
                 FcitxKeyState_Super)) {
        return false;
    } else if (sym > FcitxKey_space && sym <= FcitxKey_asciitilde) {
        return true;
    } else {
        return false;
    }
}


INPUT_RETURN_VALUE BogoOnKeyPress(Bogo *self,
                                  FcitxKeySym sym,
                                  unsigned int state)
{
    if (CanProcess(sym, state)) {
        // Convert the keysym to UTF8
        char *sym_utf8 = malloc(UTF8_MAX_LENGTH + 1);
        memset(sym_utf8, 0, UTF8_MAX_LENGTH + 1);
        FcitxUnikeyUcs4ToUtf8(self, sym, sym_utf8);

        // Append the key to raw_string
        if (strlen(self->raw_string) + strlen(sym_utf8) > self->raw_string_len) {
            // FIXME: Realloc can fail
            realloc(self->raw_string, self->raw_string_len * 2);
        }
        strcat(self->raw_string, sym_utf8);
        LOG("keysym: %s\n", sym_utf8);
        free(sym_utf8);

        // Send the raw key sequence to bogo-python to get the
        // converted string.
        PyObject *args, *pyResult;

        args = Py_BuildValue("(s)", self->raw_string);
        pyResult = PyObject_CallObject(bogo_process_sequence_func, args);
        Py_DECREF(args);

        char *result = strdup(PyUnicode_AsUTF8(pyResult));
        Py_DECREF(pyResult);

        return CommitString(self, result);
    }

    BogoOnReset(self);
    return IRV_TO_PROCESS;
}


INPUT_RETURN_VALUE CommitString(Bogo *self, char *str) {
    // Find the number of same chars between str and previous_result
    int byte_offset = 0;
    int same_chars = 0;
    int char_len = 0;
    
    while (true) {
        char_len = fcitx_utf8_char_len(
            self->previous_result + byte_offset);

        if (strncmp(self->previous_result + byte_offset,
                    str + byte_offset, char_len) != 0) {
            // same_chars and byte_offset are the results of this
            // loop.
            break;
        }

        byte_offset += char_len;
        same_chars++;
    }

    // The number of backspaces to send is the number of UTF8 chars
    // at the end of previous_result that differ from result.
    int num_backspace = 0;
    num_backspace = \
        fcitx_utf8_strlen(self->previous_result) - same_chars;

    LOG("num_backspace: %d\n", num_backspace);

    char *string_to_commit = str + byte_offset;

    FcitxInputContext *ic = FcitxInstanceGetCurrentIC(self->fcitx);

    if (SupportSurroundingText(self)) {
        LOG("Delete surrounding text\n");
        FcitxInstanceDeleteSurroundingText(
                    self->fcitx,
                    ic,
                    -num_backspace,
                    num_backspace);
    } else {
        LOG("Send backspaces\n");
        int i = 0;
        for (; i < num_backspace; ++i) {
            FcitxInstanceForwardKey(
                        self->fcitx,
                        ic,
                        FCITX_PRESS_KEY,
                        FcitxKey_BackSpace,
                        0);
            
            FcitxInstanceForwardKey(
                        self->fcitx,
                        ic,
                        FCITX_RELEASE_KEY,
                        FcitxKey_BackSpace,
                        0);
        }

        // Delay to make sure all the backspaces have been processed.
        if (num_backspace > 0) {
            struct timespec sleepTime = {
                0,
                20 * 1000000 // 20 miliseconds
            };

            nanosleep(&sleepTime, NULL);
        }
    }

    FcitxInstanceCommitString(
                self->fcitx,
                FcitxInstanceGetCurrentIC(self->fcitx),
                string_to_commit);

    free(self->previous_result);
    self->previous_result = str;

    return IRV_FLAG_BLOCK_FOLLOWING_PROCESS;
}


void BogoOnSave(Bogo *self)
{
    LOG("Saved\n");
}

void BogoOnConfig(Bogo *self)
{
    LOG("Reload config\n");
}

int FcitxUnikeyUcs4ToUtf8(Bogo *self,
                          const unsigned int c,
                          char buf[UTF8_MAX_LENGTH + 1])
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

boolean SupportSurroundingText(Bogo *self)
{
    char *badPrograms[] = {
        "firefox", "konsole"
    };
    
    FcitxInputContext *ic = FcitxInstanceGetCurrentIC(self->fcitx);
    FcitxInputContext2 *ic2 = (FcitxInputContext2 *) ic;
    
    char *prgname = ic2->prgname;
    LOG("prgname: %s\n", prgname);

    boolean support = ic->contextCaps & CAPACITY_SURROUNDING_TEXT;

    if (support) {
        int i;
        for (i = 0; i < sizeof(badPrograms) / sizeof(char *); i++) {
            if (strcmp(badPrograms[i], prgname) == 0) {
                support = false;
                break;
            }
        }
    }

    return support;
}

