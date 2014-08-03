#include <fcitx/fcitx.h>
#include <fcitx/ime.h>
#include <fcitx/instance.h>


#define LOGGING

#ifdef LOGGING
    #define LOG(fmt...) printf(fmt)
#else
    #define LOG(fmt...)
#endif


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
    int nothing;
} Bogo;


// Public interface functions
static boolean FcitxUnikeyInit(void* arg);
static INPUT_RETURN_VALUE FcitxBogoDoInput(void* arg, FcitxKeySym sym, unsigned int state);
static void FcitxUnikeyReset(void* arg);
static void FcitxUnikeySave(void* arg);
static void ReloadConfigFcitxUnikey(void* arg);


void* FcitxBogoSetup(FcitxInstance* instance)
{
    Bogo *bogo = malloc(sizeof(Bogo));
    memset(bogo, 0, sizeof(Bogo));
    
    bogo->fcitx = instance;
    bogo->nothing = 1;

    FcitxIMIFace iface;
    memset(&iface, 0, sizeof(FcitxIMIFace));

    iface.Init = FcitxUnikeyInit;
    iface.ResetIM = FcitxUnikeyReset;
    iface.DoInput = FcitxBogoDoInput;
    iface.ReloadConfig = ReloadConfigFcitxUnikey;
    iface.Save = FcitxUnikeySave;

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

    return bogo;
}

void FcitxBogoTeardown(void* arg)
{
    LOG("Destroyed\n");
}

boolean FcitxUnikeyInit(void* arg)
{
    LOG("Init\n");
    return true;
}

INPUT_RETURN_VALUE FcitxBogoDoInput(void* arg, FcitxKeySym sym, unsigned int state)
{
    Bogo *self = (Bogo *) arg;
    LOG("%d\n", self->nothing);
    return IRV_FLAG_FORWARD_KEY;
}

void FcitxUnikeyReset(void* arg)
{
    LOG("Reset\n");
}

void FcitxUnikeySave(void* arg)
{
    LOG("Saved\n");
}

void ReloadConfigFcitxUnikey(void* arg)
{
    LOG("Reload config\n");
}
