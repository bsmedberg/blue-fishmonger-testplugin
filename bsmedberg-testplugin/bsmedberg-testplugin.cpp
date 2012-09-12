// bsmedberg-testplugin.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

static NPNetscapeFuncs* sBrowserFuncs;

struct InstanceData
{
    InstanceData(NPP i)
        : npp(i)
        , proto(NULL)
        , window()
    { }

    NPP npp;
    NPObject* proto;
    NPWindow window;
};

static InstanceData* NPPToInstance(NPP npp)
{
    return static_cast<InstanceData*>(npp->pdata);
}

static NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
        int16_t argc, char* argn[], char* argv[],
        NPSavedData*)
{
    instance->pdata = new InstanceData(instance);
    NPN_SetValue(instance, NPPVpluginWindowBool, (void*) false);
    return NPERR_NO_ERROR;
}

static NPError
NPP_Destroy(NPP instance, NPSavedData**)
{
    InstanceData* id = NPPToInstance(instance);
    if (id->proto) {
        NPN_ReleaseObject(id->proto);
        id->proto = NULL;
    }
    return NPERR_NO_ERROR;
}

static NPError
NPP_SetWindow(NPP instance, NPWindow* window)
{
    InstanceData* id = NPPToInstance(instance);
    if (id->window.width != window->width ||
        id->window.height != window->height) {
        NPRect r = { 0, 0, window->height, window->width };
        NPN_InvalidateRect(instance, &r);
    }
    id->window = *window;
    return NPERR_NO_ERROR;
}

static NPError
NPP_NewStream(NPP, NPMIMEType, NPStream*, NPBool, uint16_t*)
{
    return NPERR_GENERIC_ERROR;
}

static void
PluginDraw(InstanceData* id)
{
    NPP npp = id->npp;

    HDC hdc = (HDC) id->window.window;

    int savedDCID = SaveDC(hdc);

    const RECT fill = { id->window.x, id->window.y, id->window.x + id->window.width, id->window.y + id->window.height };
    SetBkMode(hdc, TRANSPARENT);
    HBRUSH brush = ::CreateSolidBrush(RGB(192, 120, 50));
    FillRect(hdc, &fill, brush);
    DeleteObject(brush);

    if (id->window.width > 2 && id->window.height > 2) {
        brush = CreateSolidBrush(RGB(212, 192, 150));
        const RECT inset = { id->window.x + 1, id->window.y + 1, id->window.x + id->window.width - 1, id->window.y + id->window.height - 1 };
        FillRect(hdc, &inset, brush);
        DeleteObject(brush);
    }

    if (id->window.width > 6 && id->window.height > 6) {
        HFONT font = CreateFont(20, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, 5, DEFAULT_PITCH, L"Arial");
        SelectObject(hdc, font);
        RECT inset = { id->window.x + 3, id->window.y + 3, id->window.x + id->window.width - 3, id->window.y + id->window.height - 3 };
        DrawText(hdc, L"Hello, world.", -1, &inset, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
        DeleteObject(font);
    }

    RestoreDC(hdc, savedDCID);
}

static int16_t
NPP_HandleEvent(NPP instance, void* event)
{
    InstanceData* id = NPPToInstance(instance);

    NPEvent* pe = static_cast<NPEvent*>(event);
    switch ((UINT) pe->event) {
    case WM_PAINT:
        PluginDraw(id);
        return true;

#if 0
    case WM_CHAR:
        DoArrayTest(id);
        return true;
#endif
    }
    return false;
}

struct ProtoStruct : public NPObject
{
    ProtoStruct(NPP npp)
        : id(NPPToInstance(npp))
    { }
    InstanceData* id;
};

static InstanceData*
InstanceFromProto(NPObject* obj)
{
    return static_cast<ProtoStruct*>(obj)->id;
}

static NPObject*
ProtoAllocate(NPP npp, NPClass* aClass)
{
    return new ProtoStruct(npp);
}

static void
ProtoDeallocate(NPObject* obj)
{
    delete obj;
}

static bool
ProtoHasMethod(NPObject* obj, NPIdentifier name)
{
    if (NPN_GetStringIdentifier("bsmedbergcall") == name)
        return true;

    return false;
}

static bool
ProtoInvoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result)
{
    static const char sNewArray[] = "new Array;";
    static const NPString str = { sNewArray, sizeof(sNewArray) - 1 };
    return NPN_Evaluate(InstanceFromProto(obj)->npp, obj, const_cast<NPString*>(&str), result);
}

static bool
ProtoHasProperty(NPObject* obj, NPIdentifier name)
{
    if (NPN_GetStringIdentifier("bsmedbergprop") == name)
        return true;
    return false;
}

static bool
ProtoGetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    if (NPN_GetStringIdentifier("bsmedbergprop") == name) {
        INT32_TO_NPVARIANT(42, *result);
        return true;
    }
    return false;
}

static const NPClass sProtoNPClass =
{
    NP_CLASS_STRUCT_VERSION,
    ProtoAllocate,
    ProtoDeallocate,
    NULL,
    ProtoHasMethod,
    ProtoInvoke,
    NULL,
    ProtoHasProperty,
    ProtoGetProperty,
    NULL,
    NULL,
    NULL,
    NULL
};

static NPError
NPP_GetValue(NPP instance, NPPVariable variable, void* value)
{
    InstanceData* id = NPPToInstance(instance);
    if (variable == NPPVpluginScriptableNPObject) {
        NPObject* obj = NPN_CreateObject(instance, const_cast<NPClass*>(&sProtoNPClass));
        id->proto = NPN_RetainObject(obj);
        *static_cast<NPObject**>(value) = obj;
        return NPERR_NO_ERROR;
    }
    return NPERR_GENERIC_ERROR;
}

static void FillPluginFunctionTable(NPPluginFuncs* pFuncs)
{
    pFuncs->newp = NPP_New;
    pFuncs->destroy = NPP_Destroy;
    pFuncs->setwindow = NPP_SetWindow;
    pFuncs->newstream = NPP_NewStream;
    pFuncs->event = NPP_HandleEvent;
    pFuncs->getvalue = NPP_GetValue;
}

NPError OSCALL NP_Initialize(NPNetscapeFuncs* bFuncs)
{
    sBrowserFuncs = bFuncs;
    return NPERR_NO_ERROR;
}

NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* pFuncs)
{
    FillPluginFunctionTable(pFuncs);
    return NPERR_NO_ERROR;
}

NPError OSCALL NP_Shutdown()
{
    return NPERR_NO_ERROR;
}

const char* NP_GetMIMEDescription()
{
    return "application/x-bsmedberg-test:bstest:bsmedberg Test Plugin";
}

// browserfuncs forwards

static NPObject*
NPN_CreateObject(NPP instance, NPClass* c)
{
    return sBrowserFuncs->createobject(instance, c);
}

static bool
NPN_Evaluate(NPP instance, NPObject* obj, NPString* script, NPVariant* result)
{
    return sBrowserFuncs->evaluate(instance, obj, script, result);
}

static NPIdentifier
NPN_GetStringIdentifier(const NPUTF8* name)
{
    return sBrowserFuncs->getstringidentifier(name);
}

static void
NPN_InvalidateRect(NPP instance, NPRect* rect)
{
    sBrowserFuncs->invalidaterect(instance, rect);
}

static void
NPN_ReleaseObject(NPObject* o)
{
    sBrowserFuncs->releaseobject(o);
}

static NPObject*
NPN_RetainObject(NPObject* o)
{
    return sBrowserFuncs->retainobject(o);
}

static NPError
NPN_SetValue(NPP instance, NPPVariable variable, void* value)
{
    return sBrowserFuncs->setvalue(instance, variable, value);
}

