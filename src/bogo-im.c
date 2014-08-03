#include <fcitx/fcitx.h>
#include <fcitx/ime.h>
#include <fcitx/instance.h>

static void* FcitxBogoCreate(FcitxInstance* instance);
static void FcitxBogoDestroy(void* arg);

FCITX_EXPORT_API
FcitxIMClass ime = {
    FcitxBogoCreate,
    FcitxBogoDestroy
};
FCITX_EXPORT_API
int ABI_VERSION = FCITX_ABI_VERSION; 


// Public interface functions
static boolean FcitxUnikeyInit(void* arg);
static INPUT_RETURN_VALUE FcitxBogoDoInput(void* arg, FcitxKeySym sym, unsigned int state);
static void FcitxUnikeyReset(void* arg);
static void FcitxUnikeySave(void* arg);
static void ReloadConfigFcitxUnikey(void* arg);


typedef struct {
    // The handle to talk to fcitx
    FcitxInstance *fcitx;
    int nothing;
} Bogo;

#define LOGGING

#ifdef LOGGING
    #define LOG(fmt...) printf(fmt)
#else
    #define LOG(fmt...)
#endif


void* FcitxBogoCreate(FcitxInstance* instance)
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
        instance,
        bogo,
        "bogo",
        "Bogo",
        "unikey",
        iface,
        1,
        "vi"
    );

    return bogo;
}

void FcitxBogoDestroy(void* arg)
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
