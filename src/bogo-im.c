#include <Python.h>
#include <fcitx/fcitx.h>
#include <fcitx/ime.h>
#include <fcitx/instance.h>
#include <fcitx-utils/utf8.h>
#include <iconv.h>
#include <time.h>

#include "config.h"
#include "python3compat.h"


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

#ifdef DEBUG
    #define LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#else
    #define LOG(fmt...)
#endif

#define INITIAL_STRING_LEN 128


#ifdef LIBICONV_SECOND_ARGUMENT_IS_CONST
typedef const char* IconvStr;
#else
typedef char* IconvStr;
#endif

static PyObject *bogo_process_sequence_func;
static PyObject *bogo_handle_backspace_func;


typedef enum {
    DELETE_METHOD_BACKSPACE,
    DELETE_METHOD_SURROUNDING_TEXT
} DELETE_METHOD;


typedef struct {
    // The handle to talk to fcitx
    FcitxInstance *fcitx;
    char *rawString;
    int rawStringLen;
    
    char *prevConvertedString;
} Bogo;

int Utf32ToUtf8Char(const uint32_t c, char buf[UTF8_MAX_LENGTH + 1]);
uint32_t Utf8ToUtf32Char(char *src);


// Public interface functions
static boolean BogoOnInit(Bogo *self);
static INPUT_RETURN_VALUE BogoOnKeyPress(Bogo *self,
                                         FcitxKeySym sym,
                                         unsigned int state);
static void BogoOnReset(Bogo *self);
static void BogoOnSave(Bogo *self);
static void BogoOnConfig(Bogo *self);

static boolean SupportSurroundingText(Bogo *self);
static void CommitString(Bogo *self, char *str);
static char *ProgramName(Bogo *self);
static DELETE_METHOD DeletePreviousChars(Bogo *self, int num_backspace);
static void CommitStringByForwarding(Bogo *self, const char *str);
static boolean IsGtkAppNotSupportingSurroundingText(char *name);
static boolean IsQtAppNotSupportingSurroundingText(char *name);


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

    // Load the bogo-python engine
#if (PY_VERSION_HEX < 0x03000000)
    Py_SetProgramName("fcitx-bogo");
#else
    Py_SetProgramName(L"fcitx-bogo");
#endif
    Py_Initialize();

    PyRun_SimpleString(
        "import sys; sys.path.append('" DATA_INSTALL_PATH "')");

    PyObject *moduleName, *bogoModule;
    moduleName = PyUnicode_FromString("bogo");
    bogoModule = PyImport_Import(moduleName);
    Py_DECREF(moduleName);

    bogo_process_sequence_func = \
        PyObject_GetAttrString(bogoModule, "process_sequence");
    
    bogo_handle_backspace_func = \
        PyObject_GetAttrString(bogoModule, "handle_backspace");

    return bogo;
}

void FcitxBogoTeardown(void* arg)
{
    LOG("Destroyed");
    Py_Finalize();
}


void BogoInitialize(Bogo *self) {
    self->prevConvertedString = malloc(1);
    self->prevConvertedString[0] = 0;
    self->rawString = malloc(INITIAL_STRING_LEN);
    self->rawString[0] = 0;
    self->rawStringLen = INITIAL_STRING_LEN;
}


boolean BogoOnInit(Bogo *self)
{
    LOG("Init");
    BogoInitialize(self);
    return true;
}


void BogoOnReset(Bogo *self)
{
    LOG("Reset");
    if (self->prevConvertedString) {
        free(self->prevConvertedString);
    }

    if (self->rawString) {
        free(self->rawString);
    }

    BogoInitialize(self);
}


boolean CanProcess(FcitxKeySym sym, unsigned int state)
{
    if (state & (FcitxKeyState_Ctrl |
                 FcitxKeyState_Alt |
                 FcitxKeyState_Super)) {
        return false;
    } else if (sym >= FcitxKey_space && sym <= FcitxKey_asciitilde) {
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
        char sym_utf8[UTF8_MAX_LENGTH + 1];
        memset(sym_utf8, 0, UTF8_MAX_LENGTH + 1);
    
        Utf32ToUtf8Char(sym, sym_utf8);
        LOG("keysym: %s", sym_utf8);
    
        // Append the key to raw_string
        if (strlen(self->rawString) + strlen(sym_utf8) > 
                self->rawStringLen) {
            char *tmp = realloc(self->rawString,
                                self->rawStringLen * 2);
            if (tmp != NULL) {
                self->rawString = tmp;
                self->rawStringLen = self->rawStringLen * 2;
            }
        }
        strcat(self->rawString, sym_utf8);
    
        // Send the raw key sequence to bogo-python to get the
        // converted string.
        PyObject *args, *pyResult;
    
        args = Py_BuildValue("(s)", self->rawString);
        pyResult = PyObject_CallObject(bogo_process_sequence_func,
                                       args);
    
        char *convertedString = strdup(PyUnicode_AsUTF8(pyResult));
        
        Py_DECREF(args);
        Py_DECREF(pyResult);

        CommitString(self, convertedString);

        return IRV_FLAG_BLOCK_FOLLOWING_PROCESS;
    } else if (sym == FcitxKey_BackSpace) {
        if (strlen(self->rawString) > 0) {
            PyObject *args, *result, *newConvertedString, *newRawString,
                    *prevConvertedString, *rawString;
            
            prevConvertedString = \
                PyUnicode_FromString(self->prevConvertedString);
            rawString = PyUnicode_FromString(self->rawString);
            
            args = PyTuple_Pack(2, prevConvertedString, rawString);

            result = PyObject_CallObject(bogo_handle_backspace_func,
                                         args);

            newConvertedString = PyTuple_GetItem(result, 0);
            newRawString = PyTuple_GetItem(result, 1);

            strcpy(self->rawString, PyUnicode_AsUTF8(newRawString));
            CommitString(self,
                         strdup(PyUnicode_AsUTF8(newConvertedString)));

            Py_DECREF(result);
            Py_DECREF(args);

            return IRV_FLAG_BLOCK_FOLLOWING_PROCESS;
        } else {
            return IRV_FLAG_FORWARD_KEY;
        }
    } else {
        BogoOnReset(self);
        return IRV_TO_PROCESS;
    }
}


void CommitString(Bogo *self, char *str) {
    // Find the number of same chars between str and previous_result
    int byte_offset = 0;
    int same_chars = 0;
    int char_len = 0;
    
    while (true) {
        char_len = fcitx_utf8_char_len(
            self->prevConvertedString + byte_offset);

        if (strncmp(self->prevConvertedString + byte_offset,
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
        fcitx_utf8_strlen(self->prevConvertedString) - same_chars;

    LOG("num_backspace: %d\n", num_backspace);
    DELETE_METHOD method = DeletePreviousChars(self, num_backspace);

    char *string_to_commit = str + byte_offset;
    
    if (IsGtkAppNotSupportingSurroundingText(ProgramName(self))) {
        LOG("Commit string by forwarding");
        CommitStringByForwarding(self, string_to_commit);
    } else {
        if (method == DELETE_METHOD_BACKSPACE &&
            !IsQtAppNotSupportingSurroundingText(ProgramName(self)))
        {
            LOG("Delaying");
            // Delay to make sure all the backspaces have been 
            // processed.
            // FIXME 30 is just a magic number found through
            //       trial-and-error. Maybe we should allow it to be
            //       user-configurable.
            if (num_backspace > 0) {
                struct timespec sleepTime = {
                    0,
                    30 * 1000000 // 30 miliseconds
                };

                nanosleep(&sleepTime, NULL);
            }
        }

        FcitxInstanceCommitString(
                    self->fcitx,
                    FcitxInstanceGetCurrentIC(self->fcitx),
                    string_to_commit);
    }

    free(self->prevConvertedString);
    self->prevConvertedString = str;
}


DELETE_METHOD DeletePreviousChars(Bogo *self, int num_backspace)
{
    FcitxInputContext *ic = FcitxInstanceGetCurrentIC(self->fcitx);
    if (SupportSurroundingText(self)) {
        LOG("Delete surrounding text");
        FcitxInstanceDeleteSurroundingText(
                    self->fcitx,
                    ic,
                    -num_backspace,
                    num_backspace);
        return DELETE_METHOD_SURROUNDING_TEXT;
    } else {
        LOG("Send backspaces");
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
        return DELETE_METHOD_BACKSPACE;
    }
}


void BogoOnSave(Bogo *self)
{
    LOG("Saved");
}

void BogoOnConfig(Bogo *self)
{
    LOG("Reload config");
}


char *ProgramName(Bogo *self)
{
    FcitxInputContext2 *ic = 
        (FcitxInputContext2 *) FcitxInstanceGetCurrentIC(self->fcitx);
    if (ic->prgname) {
        return ic->prgname;
    } else {
        return "";
    }
}


boolean IsStringInCollection(char *str, char **collection, int length)
{
    int i;
    for (i = 0; i < length; i++) {
        if (strcmp(collection[i], str) == 0) {
            return true;
        }
    }
    return false;
}


boolean IsGtkAppNotSupportingSurroundingText(char *name)
{
    char *names[] = {
        "firefox", "terminator", "gnome-terminal-", "mate-terminal",
        "lxterminal", "geany"
    };

    return IsStringInCollection(name,
                                names,
                                sizeof(names) / sizeof(char *));
}

boolean IsQtAppNotSupportingSurroundingText(char *name)
{
    char *names[] = {
        "konsole"
    };

    return IsStringInCollection(name,
                                names,
                                sizeof(names) / sizeof(char *));
}


boolean SupportSurroundingText(Bogo *self)
{
    FcitxInputContext *ic = FcitxInstanceGetCurrentIC(self->fcitx);

    char *prgname = ProgramName(self);
    LOG("prgname: %s", prgname);

    boolean support = ic->contextCaps & CAPACITY_SURROUNDING_TEXT;

    return support && 
           !IsGtkAppNotSupportingSurroundingText(prgname) &&
           !IsQtAppNotSupportingSurroundingText(prgname);
}


void CommitStringByForwarding(Bogo *self, const char *str)
{
    int len;
    char chr[UTF8_MAX_LENGTH + 1];
    FcitxInputContext *ic = FcitxInstanceGetCurrentIC(self->fcitx);
    int offset = 0;

    while(str[offset] != 0) {
        len = fcitx_utf8_char_len(str + offset);
        strncpy(chr, str + offset, len);
        chr[len] = 0;
        LOG("%s %d %d", chr, len, offset);
        
        uint32_t utf32 = Utf8ToUtf32Char(chr);
        FcitxKeySym keysym = FcitxUnicodeToKeySym(utf32);
        LOG("%d", keysym);
        
        FcitxInstanceForwardKey(
                    self->fcitx,
                    ic,
                    FCITX_PRESS_KEY,
                    keysym,
                    0);
        
        FcitxInstanceForwardKey(
                    self->fcitx,
                    ic,
                    FCITX_RELEASE_KEY,
                    keysym,
                    0);

        offset += len;
    }
}


int Utf32ToUtf8Char(const uint32_t c, char buf[UTF8_MAX_LENGTH + 1])
{
    unsigned int str[2];
    str[0] = c;
    str[1] = 0;
    
    iconv_t *conv;
    
    union {
        short s;
        unsigned char b[2];
    } endian;
    
    endian.s = 0x1234;
    if (endian.b[0] == 0x12)
        conv = iconv_open("utf-8", "ucs-4be");
    else
        conv = iconv_open("utf-8", "ucs-4le");

    size_t ucslen = 1;
    size_t len = UTF8_MAX_LENGTH;
    len *= sizeof(char);
    ucslen *= sizeof(unsigned int);
    char* p = buf;
    IconvStr src = (IconvStr) str;
    iconv(conv, &src, &ucslen, &p, &len);
    
    iconv_close(conv);
    return (UTF8_MAX_LENGTH - len) / sizeof(char);
}


uint32_t Utf8ToUtf32Char(char *src)
{
    iconv_t *conv;

    union {
        short s;
        unsigned char b[2];
    } endian;

    endian.s = 0x1234;
    if (endian.b[0] == 0x12)
        conv = iconv_open("ucs-4be", "utf-8");
    else
        conv = iconv_open("ucs-4le", "utf-8");
    
    uint32_t output[2];
    
    size_t inLength = UTF8_MAX_LENGTH + 1;
    size_t outLength = sizeof(uint32_t);
    IconvStr buff = (IconvStr) output;

    iconv(conv, &src, &inLength, &buff, &outLength);
    
    iconv_close(conv);
    return output[0];
}
