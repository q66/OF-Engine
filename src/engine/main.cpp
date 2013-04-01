// main.cpp: initialisation & main loop

#include "engine.h"

#include "client_system.h" // INTENSITY
#include "editing_system.h" // INTENSITY
#include "message_system.h"
#include "of_localserver.h"
#include "of_tools.h"

extern void cleargamma();

void cleanup()
{
    recorder::stop();
    cleanupserver();
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_TRUE);
    cleargamma();
    freeocta(worldroot);
    extern void clear_console(); clear_console();
    extern void clear_mdls();    clear_mdls();
    extern void clear_sound();   clear_sound();
    closelogfile();
    SDL_Quit();
}

void quit()                     // normal exit
{
    extern void writeinitcfg();
    writeinitcfg();

    abortconnect();
    disconnect();
    localdisconnect();
    tools::writecfg();
    cleanup();
    exit(EXIT_SUCCESS);
}

void fatal(const char *s, ...)    // failure exit
{
    static int errors = 0;
    errors++;

    if(errors <= 2) // print up to one extra recursive error
    {
        defvformatstring(msg,s,s);
        logoutf("%s", msg);

        if(errors <= 1) // avoid recursion
        {
            if(SDL_WasInit(SDL_INIT_VIDEO))
            {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_TRUE);
                cleargamma();
            }
            #ifdef WIN32
                MessageBox(NULL, msg, "OctaForge fatal error", MB_OK|MB_SYSTEMMODAL);
            #endif
            SDL_Quit();
        }
    }

    exit(EXIT_FAILURE);
}

SDL_Window *screen = NULL;
int screenw = 0, screenh = 0;
SDL_GLContext glcontext = NULL;

int curtime = 0, totalmillis = 1, lastmillis = 1;

dynent *player = NULL;

int initing = NOT_INITING;

bool initwarning(const char *desc, int level, int type)
{
    if(initing < level) 
    {
        lapi::state.get<lua::Function>("external", "change_add")
            (desc, type);
        return true;
    }
    return false;
}

#define SCR_MINW 320
#define SCR_MINH 200
#define SCR_MAXW 10000
#define SCR_MAXH 10000
#define SCR_DEFAULTW 1024
#define SCR_DEFAULTH 768
VARF(scr_w, SCR_MINW, -1, SCR_MAXW, initwarning("screen resolution"));
VARF(scr_h, SCR_MINH, -1, SCR_MAXH, initwarning("screen resolution"));

void writeinitcfg()
{
    stream *f = openutf8file("init.lua", "w");
    if(!f) return;
    f->printf("-- automatically written on exit, DO NOT MODIFY\n-- modify settings in game\n");
    extern int fullscreen, sound, soundchans, soundfreq, soundbufferlen;
    f->printf("EV.fullscreen = %d\n", fullscreen);
    f->printf("EV.scr_w = %d\n", scr_w);
    f->printf("EV.scr_h = %d\n", scr_h);
    f->printf("EV.sound = %d\n", sound);
    f->printf("EV.soundchans = %d\n", soundchans);
    f->printf("EV.soundfreq = %d\n", soundfreq);
    f->printf("EV.soundbufferlen = %d\n", soundbufferlen);
    delete f;
}

static void getbackgroundres(int &w, int &h)
{
    float wk = 1, hk = 1;
    if(w < 1024) wk = 1024.0f/w;
    if(h < 768) hk = 768.0f/h;
    wk = hk = max(wk, hk);
    w = int(ceil(w*wk));
    h = int(ceil(h*hk));
}

string backgroundcaption = "";
Texture *backgroundmapshot = NULL;
string backgroundmapname = "";
char *backgroundmapinfo = NULL;

void restorebackground()
{
    if(renderedframe) return;
    renderbackground(backgroundcaption[0] ? backgroundcaption : NULL, backgroundmapshot, backgroundmapname[0] ? backgroundmapname : NULL, backgroundmapinfo, true);
}

void bgquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1)
{
    varray::begin(GL_TRIANGLE_STRIP);
    varray::attribf(x,   y);   varray::attribf(tx,      ty);
    varray::attribf(x+w, y);   varray::attribf(tx + tw, ty);
    varray::attribf(x,   y+h); varray::attribf(tx,      ty + th);
    varray::attribf(x+w, y+h); varray::attribf(tx + tw, ty + th);
    varray::end();
}

void renderbackground(const char *caption, Texture *mapshot, const char *mapname, const char *mapinfo, bool restore, bool force)
{
    if(!inbetweenframes && !force) return;

    stopsounds(); // stop sounds while loading
 
    int w = screenw, h = screenh;
    getbackgroundres(w, h);
    gettextres(w, h);

    static int lastupdate = -1, lastw = -1, lasth = -1;
    static float backgroundu = 0, backgroundv = 0;
#if 0
    static float detailu = 0, detailv = 0;
    static int numdecals = 0;
    static struct decal { float x, y, size; int side; } decals[12];
#endif
    if((renderedframe && !gui_mainmenu && lastupdate != lastmillis) || lastw != w || lasth != h)
    {
        lastupdate = lastmillis;
        lastw = w;
        lasth = h;

        backgroundu = rndscale(1);
        backgroundv = rndscale(1);
#if 0
        detailu = rndscale(1);
        detailv = rndscale(1);
        numdecals = sizeof(decals)/sizeof(decals[0]);
        numdecals = numdecals/3 + rnd((numdecals*2)/3 + 1);
        float maxsize = min(w, h)/16.0f;
        loopi(numdecals)
        {
            decal d = { rndscale(w), rndscale(h), maxsize/2 + rndscale(maxsize/2), rnd(2) };
            decals[i] = d;
        }
#endif
    }
    else if(lastupdate != lastmillis) lastupdate = lastmillis;

    loopi(restore ? 1 : 3)
    {
        hudmatrix.ortho(0, w, h, 0, -1, 1);
        resethudmatrix();
        hudshader->set();

        varray::defvertex(2);
        varray::deftexcoord0();

        varray::colorf(1, 1, 1);
        settexture("data/textures/ui/background.png", 0);
        float bu = w*0.67f/256.0f + backgroundu, bv = h*0.67f/256.0f + backgroundv;
        bgquad(0, 0, w, h, 0, 0, bu, bv);
        glEnable(GL_BLEND);
#if 0
        settexture("<premul>data/textures/ui/background_detail.png", 0);
        float du = w*0.8f/512.0f + detailu, dv = h*0.8f/512.0f + detailv;
        bgquad(0, 0, w, h, 0, 0, du, dv);
        settexture("<premul>data/textures/ui/background_decal.png", 3);
        loopj(numdecals)
        {
            float hsz = decals[j].size, hx = clamp(decals[j].x, hsz, w-hsz), hy = clamp(decals[j].y, hsz, h-hsz), side = decals[j].side;
            bgquad(hx-hsz, hy-hsz, 2*hsz, 2*hsz, side, 0, 1-2*side, 1);
        }
#endif
        float lh = 0.5f*min(w, h), lw = lh*2,
              lx = 0.5f*(w - lw), ly = 0.5f*(h*0.5f - lh);
        settexture(/*(maxtexsize ? min(maxtexsize, hwtexsize) : hwtexsize) >= 1024 && (screen->w > 1280 || screen->h > 800) ? "<premul>data/logo_1024.png" :*/ "<premul>data/textures/ui/logo.png", 3);
        bgquad(lx, ly, lw, lh);

#if 0
        float bh = 0.1f*min(w, h), bw = bh*2,
              bx = w - 1.1f*bw, by = h - 1.1f*bh;
        settexture("<premul>data/textures/ui/cube2badge.png", 3);
        bgquad(bx, by, bw, bh);
#endif

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if(caption)
        {
            int tw = text_width(caption);
            float tsz = 0.04f*min(w, h)/FONTH,
                  tx = 0.5f*(w - tw*tsz), ty = h - 0.075f*1.5f*min(w, h) - 1.25f*FONTH*tsz;
            pushhudmatrix();
            hudmatrix.translate(tx, ty, 0);
            hudmatrix.scale(tsz, tsz, 1);
            flushhudmatrix();
            draw_text(caption, 0, 0);
            pophudmatrix();
        }
        if(mapshot || mapname)
        {
            int infowidth = 12*FONTH;
            float sz = 0.35f*min(w, h), msz = (0.75f*min(w, h) - sz)/(infowidth + FONTH), x = 0.5f*(w-sz), y = ly+lh - sz/15;
            if(mapinfo)
            {
                int mw, mh;
                text_bounds(mapinfo, mw, mh, infowidth);
                x -= 0.5f*(mw*msz + FONTH*msz);
            }
            if(mapshot && mapshot!=notexture)
            {
                glBindTexture(GL_TEXTURE_2D, mapshot->id);
                bgquad(x, y, sz, sz);
            }
            else
            {
                int qw, qh;
                text_bounds("?", qw, qh);
                float qsz = sz*0.5f/max(qw, qh);
                pushhudmatrix();
                hudmatrix.translate(x + 0.5f*(sz - qw*qsz), y + 0.5f*(sz - qh*qsz), 0);
                hudmatrix.scale(qsz, qsz, 1);
                flushhudmatrix();
                draw_text("?", 0, 0);
                pophudmatrix();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }        
            settexture("data/textures/ui/mapshot_frame.png", 3);
            bgquad(x, y, sz, sz);
            if(mapname)
            {
                int tw = text_width(mapname);
                float tsz = sz/(8*FONTH),
                      tx = 0.9f*sz - tw*tsz, ty = 0.9f*sz - FONTH*tsz;
                if(tx < 0.1f*sz) { tsz = 0.1f*sz/tw; tx = 0.1f; }
                pushhudmatrix();
                hudmatrix.translate(x+tx, y+ty, 0);
                hudmatrix.scale(tsz, tsz, 1);
                flushhudmatrix();
                draw_text(mapname, 0, 0);
                pophudmatrix();
            }
            if(mapinfo)
            {
                pushhudmatrix();
                hudmatrix.translate(x+sz+FONTH*msz, y, 0);
                hudmatrix.scale(msz, msz, 1);
                flushhudmatrix();
                draw_text(mapinfo, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, -1, infowidth);
                pophudmatrix();
            }
        }
        glDisable(GL_BLEND);

        varray::disable();

        if(!restore) swapbuffers();
    }

    if(!restore)
    {
        renderedframe = false;
        copystring(backgroundcaption, caption ? caption : "");
        backgroundmapshot = mapshot;
        copystring(backgroundmapname, mapname ? mapname : "");
        if(mapinfo != backgroundmapinfo)
        {
            DELETEA(backgroundmapinfo);
            if(mapinfo) backgroundmapinfo = newstring(mapinfo);
        }
    }
}

float loadprogress = 0;

void renderprogress(float bar, const char *text, GLuint tex, bool background)   // also used during loading
{
    if(!inbetweenframes || drawtex) return;

    clientkeepalive();      // make sure our connection doesn't time out while loading maps etc.
    
    renderbackground(NULL, backgroundmapshot, NULL, NULL, true, true); // INTENSITY

    #ifdef __APPLE__
    interceptkey(SDLK_UNKNOWN); // keep the event queue awake to avoid 'beachball' cursor
    #endif

    if(background) restorebackground();

    int w = screenw, h = screenh;
    getbackgroundres(w, h);
    gettextres(w, h);

    hudmatrix.ortho(0, w, h, 0, -1, 1);
    resethudmatrix();
    hudshader->set();

    varray::defvertex(2);
    varray::deftexcoord0();

    varray::colorf(1, 1, 1);

    float fh = 0.075f*min(w, h), fw = fh*10,
          fx = renderedframe ? w - fw - fh/4 : 0.5f*(w - fw), 
          fy = renderedframe ? fh/4 : h - fh*1.5f,
          fu1 = 0/512.0f, fu2 = 511/512.0f,
          fv1 = 0/64.0f, fv2 = 52/64.0f;

    glEnable(GL_BLEND); // INTENSITY: Moved to here, to cover loading_frame as well
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // INTENSITY: ditto

    settexture("data/textures/ui/loading_frame.png", 3);
    bgquad(fx, fy, fw, fh, fu1, fv1, fu2-fu1, fv2-fv1);

    float bw = fw*(511 - 2*17)/511.0f, bh = fh*20/52.0f,
          bx = fx + fw*17/511.0f, by = fy + fh*16/52.0f,
          bv1 = 0/32.0f, bv2 = 20/32.0f,
          su1 = 0/32.0f, su2 = 7/32.0f, sw = fw*7/511.0f,
          eu1 = 23/32.0f, eu2 = 30/32.0f, ew = fw*7/511.0f,
          mw = bw - sw - ew,
          ex = bx+sw + max(mw*bar, fw*7/511.0f);
    if(bar > 0)
    {
        settexture("data/textures/ui/loading_bar.png", 3);
        bgquad(bx, by, sw, bh, su1, bv1, su2-su1, bv2-bv1);
        bgquad(bx+sw, by, ex-(bx+sw), bh, su2, bv1, eu1-su2, bv2-bv1);
        bgquad(ex, by, ew, bh, eu1, bv1, eu2-eu1, bv2-bv1);
    }
    else if (bar < 0) // INTENSITY: Show side-to-side progress for negative values (-0 to -infinity)
    {
        float width = 0.382; // 1-golden ratio 
        float start;
        bar = -bar;
        bar = fmod(bar, 1.0f);
        if (bar < 0.5)
            start = (bar*2)*(1-width);
        else
            start = 2*(1-bar)*(1-width);

        float bw = fw*(511 - 2*17)/511.0f, bh = fh*20/52.0f,
              bx = fx + fw*17/511.0f + mw*start, by = fy + fh*16/52.0f,
              bv1 = 0/32.0f, bv2 = 20/32.0f,
              su1 = 0/32.0f, su2 = 7/32.0f, sw = fw*7/511.0f,
              eu1 = 23/32.0f, eu2 = 30/32.0f, ew = fw*7/511.0f,
              mw = bw - sw - ew,
              ex = bx+sw + max(mw*width, fw*7/511.0f);

        settexture("data/textures/ui/loading_bar.png", 3);
        bgquad(bx, by, sw, bh, su1, bv1, su2-su1, bv2-bv1);
        bgquad(bx+sw, by, ex-(bx+sw), bh, su2, bv1, eu1-su2, bv2-bv1);
        bgquad(ex, by, ew, bh, eu1, bv1, eu2-eu1, bv2-bv1);
    } // INTENSITY: End side-to-side progress


    if(text)
    {
        int tw = text_width(text);
        float tsz = bh*0.8f/FONTH;
        if(tw*tsz > mw) tsz = mw/tw;
        pushhudmatrix();
        hudmatrix.translate(bx+sw, by + (bh - FONTH*tsz)/2, 0);
        hudmatrix.scale(tsz, tsz, 1);
        flushhudmatrix();
        draw_text(text, 0, 0);
        pophudmatrix();
    }

    glDisable(GL_BLEND);

    if(tex)
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        float sz = 0.35f*min(w, h), x = 0.5f*(w-sz), y = 0.5f*min(w, h) - sz/15;
        bgquad(x, y, sz, sz);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        settexture("data/textures/ui/mapshot_frame.png", 3);
        bgquad(x, y, sz, sz);
        glDisable(GL_BLEND);
    }

    varray::disable();

    swapbuffers();
}

bool grabinput = false, minimized = false, canrelativemouse = true, relativemouse = false, allowrepeat = false;

void keyrepeat(bool on)
{
    allowrepeat = on;
}

void inputgrab(bool on)
{
    if(on)
    {
        SDL_ShowCursor(SDL_FALSE);
    #ifndef WIN32
        if(!(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            relativemouse = false;
        }
        else 
    #endif
        if(canrelativemouse)
        {
            if(SDL_SetRelativeMouseMode(SDL_TRUE) >= 0) relativemouse = true;
            else canrelativemouse = false;
        }
    }
    else 
    {
        SDL_ShowCursor(SDL_TRUE);
        if(relativemouse)
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            relativemouse = false;
        }
    }
}

void setfullscreen(bool enable)
{
    if(!screen) return;
    initwarning(enable ? "fullscreen" : "windowed");
}

VARF(fullscreen, 0, 0, 1, setfullscreen(fullscreen!=0));

void screenres(int w, int h)
{
    scr_w = clamp(w, SCR_MINW, SCR_MAXW);
    scr_h = clamp(h, SCR_MINH, SCR_MAXH);
    initwarning("screen resolution");
}

static int curgamma = 100;
VARFP(gamma, 30, 100, 300,
{
    if(gamma == curgamma) return;
    curgamma = gamma;
    if(SDL_SetWindowBrightness(screen, gamma/100.0f)==-1) conoutf(CON_ERROR, "Could not set gamma: %s", SDL_GetError());
});

void restoregamma()
{
    if(curgamma == 100) return;
    SDL_SetWindowBrightness(screen, curgamma/100.0f);
}

void cleargamma()
{
    if(curgamma != 100 && screen) SDL_SetWindowBrightness(screen, 1.0f);
}

void restorevsync()
{
    extern int vsync, vsynctear;
    if(glcontext) SDL_GL_SetSwapInterval(vsync ? (vsynctear ? -1 : 1) : 0);
}
    
VARF(vsync, 0, 0, 1, restorevsync());
VARF(vsynctear, 0, 0, 1, { if(vsync) restorevsync(); });

VAR(dbgmodes, 0, 0, 1);

void setupscreen()
{
    if(glcontext)
    {
        SDL_GL_DeleteContext(glcontext);
        glcontext = NULL;
    }
    if(screen)
    {
        SDL_DestroyWindow(screen);
        screen = NULL;
    }
    
    SDL_Rect desktop;
    memset(&desktop, 0, sizeof(desktop));
    SDL_GetDisplayBounds(0, &desktop);

    int flags = 0;
    if(fullscreen) flags = SDL_WINDOW_FULLSCREEN;
    int nummodes = SDL_GetNumDisplayModes(0);
    vector<SDL_DisplayMode> modes;
    loopi(nummodes) if(SDL_GetDisplayMode(0, i, &modes.add()) < 0) modes.drop();
    if(modes.length()) 
    {
        int widest = -1, best = -1;
        loopv(modes)
        {
            if(dbgmodes) conoutf(CON_DEBUG, "mode[%d]: %d x %d", i, modes[i].w, modes[i].h);
            if(widest < 0 || modes[i].w > modes[widest].w || (modes[i].w == modes[widest].w && modes[i].h > modes[widest].h)) 
                widest = i; 
        }
        if(scr_w < 0 || scr_h < 0)
        {
            int w = scr_w, h = scr_h, ratiow = desktop.w, ratioh = desktop.h;
            if(w < 0 && h < 0) { w = SCR_DEFAULTW; h = SCR_DEFAULTH; }
            if(ratiow <= 0 || ratioh <= 0) { ratiow = modes[widest].w; ratioh = modes[widest].h; }
            loopv(modes) if(modes[i].w*ratioh == modes[i].h*ratiow)
            {
                if(w <= modes[i].w && h <= modes[i].h && (best < 0 || modes[i].w < modes[best].w))
                    best = i;
            }
        } 
        if(best < 0)
        {
            int w = scr_w, h = scr_h;
            if(w < 0 && h < 0) { w = SCR_DEFAULTW; h = SCR_DEFAULTH; }
            else if(w < 0) w = (h*SCR_DEFAULTW)/SCR_DEFAULTH;
            else if(h < 0) h = (w*SCR_DEFAULTH)/SCR_DEFAULTW;
            loopv(modes)
            {
                if(w <= modes[i].w && h <= modes[i].h && (best < 0 || modes[i].w < modes[best].w || (modes[i].w == modes[best].w && modes[i].h < modes[best].h)))
                    best = i;
            }
        }
        if(flags&SDL_WINDOW_FULLSCREEN)
        {
            if(best >= 0) { scr_w = modes[best].w; scr_h = modes[best].h; }
            else if(desktop.w > 0 && desktop.h > 0) { scr_w = desktop.w; scr_h = desktop.h; }
            else if(widest >= 0) { scr_w = modes[widest].w; scr_h = modes[widest].h; } 
        }
        else if(best < 0)
        { 
            scr_w = min(scr_w >= 0 ? scr_w : (scr_h >= 0 ? (scr_h*SCR_DEFAULTW)/SCR_DEFAULTH : SCR_DEFAULTW), (int)modes[widest].w); 
            scr_h = min(scr_h >= 0 ? scr_h : (scr_w >= 0 ? (scr_w*SCR_DEFAULTH)/SCR_DEFAULTW : SCR_DEFAULTH), (int)modes[widest].h);
        }
        if(dbgmodes) conoutf(CON_DEBUG, "selected %d x %d", scr_w, scr_h);
    }
    if(scr_w < 0 && scr_h < 0) { scr_w = SCR_DEFAULTW; scr_h = SCR_DEFAULTH; }
    else if(scr_w < 0) scr_w = (scr_h*SCR_DEFAULTW)/SCR_DEFAULTH;
    else if(scr_h < 0) scr_h = (scr_w*SCR_DEFAULTH)/SCR_DEFAULTW;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    screen = SDL_CreateWindow("OctaForge", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, scr_w, scr_h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | flags);
    if(!screen) fatal("failed to create OpenGL window: %s", SDL_GetError());
    
    static const struct { int major, minor; } coreversions[] = { { 3, 3 }, { 3, 2 }, { 3, 1 }, { 3, 0 } };
    loopi(sizeof(coreversions)/sizeof(coreversions[0]))
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, coreversions[i].major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, coreversions[i].minor);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        glcontext = SDL_GL_CreateContext(screen);
        if(glcontext) break;
    }
    if(!glcontext)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
        glcontext = SDL_GL_CreateContext(screen);
        if(!glcontext) fatal("failed to create OpenGL context: %s", SDL_GetError());
    }

    SDL_GetWindowSize(screen, &screenw, &screenh);

    restorevsync();
}

void resetgl()
{
    lapi::state.get<lua::Function>("external", "changes_clear")((int)(CHANGE_GFX|CHANGE_SHADERS));
    renderbackground("resetting OpenGL");

    extern void cleanupva();
    extern void cleanupparticles();
    extern void cleanupdecals();
    extern void cleanupsky();
    extern void cleanupmodels();
    extern void cleanuptextures();
    extern void cleanupblendmap();
    extern void cleanuplights();
    extern void cleanupshaders();
    extern void cleanupgl();
    recorder::cleanup();
    cleanupva();
    cleanupparticles();
    cleanupdecals();
    cleanupsky();
    cleanupmodels();
    cleanuptextures();
    cleanupblendmap();
    cleanuplights();
    cleanupshaders();
    cleanupgl();
    
    setupscreen();

    inputgrab(grabinput);

    gl_init(scr_w, scr_h);

    extern void reloadfonts();
    extern void reloadtextures();
    extern void reloadshaders();
    inbetweenframes = false;
    if(!reloadtexture(*notexture) ||
       !reloadtexture("<premul>data/textures/ui/logo.png") ||
       !reloadtexture("<premul>data/textures/ui/logo_1024.png") ||
#if 0
       !reloadtexture("<premul>data/textures/ui/cube2badge.png") ||
#endif
       !reloadtexture("data/textures/ui/background.png") ||
#if 0
       !reloadtexture("<premul>data/textures/ui/background_detail.png") ||
       !reloadtexture("<premul>data/textures/ui/background_decal.png") ||
#endif
       !reloadtexture("data/textures/ui/mapshot_frame.png") ||
       !reloadtexture("data/textures/ui/loading_frame.png") ||
       !reloadtexture("data/textures/ui/loading_bar.png"))
        fatal("failed to reload core texture");
    reloadfonts();
    inbetweenframes = true;
    renderbackground("initializing...");
    restoregamma();
    initgbuffer();
    reloadshaders();
    reloadtextures();
    initlights();
    allchanged(true);
}

vector<SDL_Event> events;

void pushevent(const SDL_Event &e)
{
    events.add(e); 
}

static bool filterevent(const SDL_Event &event)
{
    switch(event.type)
    {
        case SDL_MOUSEMOTION:
            if(grabinput && !relativemouse)
            {
                if(event.motion.x == screenw / 2 && event.motion.y == screenh / 2) 
                    return false;  // ignore any motion events generated by SDL_WarpMouse
                #ifdef __APPLE__
                if(event.motion.y == 0) 
                    return false;  // let mac users drag windows via the title bar
                #endif
            }
            break;
    }
    return true;
}

static inline bool pollevent(SDL_Event &event)
{
    while(SDL_PollEvent(&event))
    {
        if(filterevent(event)) return true;
    }
    return false;
}

bool interceptkey(int sym)
{
    static int lastintercept = SDLK_UNKNOWN;
    int len = lastintercept == sym ? events.length() : 0;
    SDL_Event event;
    while(pollevent(event))
    {
        switch(event.type)
        {
            case SDL_MOUSEMOTION: break;
            default: pushevent(event); break;
        }
    }
    lastintercept = sym;
    if(sym != SDLK_UNKNOWN) for(int i = len; i < events.length(); i++)
    {
        if(events[i].type == SDL_KEYDOWN && events[i].key.keysym.sym == sym) { events.remove(i); return true; }
    }
    return false;
}

static void ignoremousemotion()
{
    SDL_Event e;
    SDL_PumpEvents();
    while(SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION));
}

static void resetmousemotion()
{
    if(grabinput && !relativemouse)
    {
        SDL_WarpMouseInWindow(screen, screenw / 2, screenh / 2);
    }
}

static void checkmousemotion(int &dx, int &dy)
{
    loopv(events)
    {
        SDL_Event &event = events[i];
        if(event.type != SDL_MOUSEMOTION)
        { 
            if(i > 0) events.remove(0, i); 
            return; 
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
    events.setsize(0);
    SDL_Event event;
    while(pollevent(event))
    {
        if(event.type != SDL_MOUSEMOTION)
        {
            events.add(event);
            return;
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
}

void checkinput()
{
    SDL_Event event;
    //int lasttype = 0, lastbut = 0;
    bool mousemoved = false; 
    while(events.length() || pollevent(event))
    {
        if(events.length()) event = events.remove(0);

        switch(event.type)
        {
            case SDL_QUIT:
                quit();
                return;

            case SDL_TEXTINPUT:
            {
                static uchar buf[SDL_TEXTINPUTEVENT_TEXT_SIZE+1];
                int len = decodeutf8(buf, int(sizeof(buf)-1), (const uchar *)event.text.text, strlen(event.text.text));
                if(len > 0) { buf[len] = '\0'; textinput((const char *)buf, len); }
                break;
            }

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if(allowrepeat || !event.key.repeat)
                    keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED);
                break;

            case SDL_WINDOWEVENT:
                switch(event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        quit();
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        inputgrab(grabinput = true);
                        break;

                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        inputgrab(grabinput = false);
                        break;

                    case SDL_WINDOWEVENT_MINIMIZED:
                        minimized = true;
                        break;

                    case SDL_WINDOWEVENT_MAXIMIZED:
                    case SDL_WINDOWEVENT_RESTORED:
                        minimized = false;
                        break;
                }
                break;

            case SDL_MOUSEMOTION:
                if(grabinput)
                {
                    int dx = event.motion.xrel, dy = event.motion.yrel;
                    checkmousemotion(dx, dy);

                    bool b1 = lapi::state.get<lua::Function>("external", "cursor_move").call<bool>(dx, dy);
                    bool b2 = lapi::state.get<lua::Function>("external", "cursor_exists").call<bool>();

                    if(!b1 && !b2)
                        mousemove(dx, dy);
                    mousemoved = true;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                //if(lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
                keypress(event.button.button >= 4 ? -2-int(event.button.button) : -int(event.button.button), event.button.state==SDL_PRESSED);
                //lasttype = event.type;
                //lastbut = event.button.button;
                break;
    
            case SDL_MOUSEWHEEL:
                if(event.wheel.y > 0) keypress(-4, true);
                else if(event.wheel.y < 0) keypress(-5, true);
                break;
        }
    }
    if(mousemoved) resetmousemotion();
}

void swapbuffers()
{
    recorder::capture();
    SDL_GL_SwapWindow(screen);
}

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);

VARF(paused, 0, 0, 1, if(multiplayer()) paused = 0);

VAR(menufps, 0, 60, 1000);
VARP(maxfps, 0, 200, 1000);

void limitfps(int &millis, int curmillis)
{
    int limit = (gui_mainmenu || minimized) && menufps ? (maxfps ? min(maxfps, menufps) : menufps) : maxfps;
    if(!limit) return;
    static int fpserror = 0;
    int delay = 1000/limit - (millis-curmillis);
    if(delay < 0) fpserror = 0;
    else
    {
        fpserror += 1000%limit;
        if(fpserror >= limit)
        {
            ++delay;
            fpserror -= limit;
        }
        if(delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        }
    }
}

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if(!ep) fatal("unknown type");
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    string out, t;
    formatstring(out)("Tesseract Win32 Exception: 0x%x [0x%x]\n\n", er->ExceptionCode, er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#ifdef _AMD64_
    STACKFRAME64 sf = {{context->Rip, 0, AddrModeFlat}, {}, {context->Rbp, 0, AddrModeFlat}, {context->Rsp, 0, AddrModeFlat}, 0};
    while(::StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
    {
        union { IMAGEHLP_SYMBOL64 sym; char symext[sizeof(IMAGEHLP_SYMBOL64) + sizeof(string)]; };
        sym.SizeOfStruct = sizeof(sym);
        sym.MaxNameLength = sizeof(symext) - sizeof(sym);
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(line);
        DWORD64 symoff;
        DWORD lineoff;
        if(SymGetSymFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &symoff, &sym) && SymGetLineFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &lineoff, &line))
#else
    STACKFRAME sf = {{context->Eip, 0, AddrModeFlat}, {}, {context->Ebp, 0, AddrModeFlat}, {context->Esp, 0, AddrModeFlat}, 0};
    while(::StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
    {
        union { IMAGEHLP_SYMBOL sym; char symext[sizeof(IMAGEHLP_SYMBOL) + sizeof(string)]; };
        sym.SizeOfStruct = sizeof(sym);
        sym.MaxNameLength = sizeof(symext) - sizeof(sym);
        IMAGEHLP_LINE line;
        line.SizeOfStruct = sizeof(line);
        DWORD symoff, lineoff;
        if(SymGetSymFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &symoff, &sym) && SymGetLineFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &lineoff, &line))
#endif
        {
            char *del = strrchr(line.FileName, '\\');
            formatstring(t)("%s - %s [%d]\n", sym.Name, del ? del + 1 : line.FileName, line.LineNumber);
            concatstring(out, t);
        }
    }
    fatal(out);
}
#endif

#define MAXFPSHISTORY 60

int fpspos = 0, fpshistory[MAXFPSHISTORY];

void resetfpshistory()
{
    loopi(MAXFPSHISTORY) fpshistory[i] = 1;
    fpspos = 0;
}

void updatefpshistory(int millis)
{
    fpshistory[fpspos++] = max(1, min(1000, millis));
    if(fpspos>=MAXFPSHISTORY) fpspos = 0;
}

void getframemillis(float &avg, float &bestdiff, float &worstdiff)
{
    int total = fpshistory[MAXFPSHISTORY-1], best = total, worst = total;
    loopi(MAXFPSHISTORY-1)
    {
        int millis = fpshistory[i];
        total += millis;
        if(millis < best) best = millis;
        if(millis > worst) worst = millis;
    }

    avg = total/float(MAXFPSHISTORY);
    best = best - avg;
    worstdiff = avg - worst;    
}

void getfps(int &fps, int &bestdiff, int &worstdiff)
{
    int total = fpshistory[MAXFPSHISTORY-1], best = total, worst = total;
    loopi(MAXFPSHISTORY-1)
    {
        int millis = fpshistory[i];
        total += millis;
        if(millis < best) best = millis;
        if(millis > worst) worst = millis;
    }

    fps = (1000*MAXFPSHISTORY)/total;
    bestdiff = 1000/best-fps;
    worstdiff = fps-1000/worst;
}

types::Tuple<int, int, int> getfps_(bool raw)
{
    int fps, bestdiff = 0, worstdiff = 0;
    if(raw) fps = 1000/fpshistory[(fpspos+MAXFPSHISTORY-1)%MAXFPSHISTORY];
    else getfps(fps, bestdiff, worstdiff);
    return types::make_tuple(fps, bestdiff, worstdiff);
}

bool inbetweenframes = false, renderedframe = true;

static bool findarg(int argc, char **argv, const char *str)
{
    for(int i = 1; i<argc; i++) if(strstr(argv[i], str)==argv[i]) return true;
    return false;
}

int clockrealbase = 0, clockvirtbase = 0; // INTENSITY: Removed 'static'
void clockreset() { clockrealbase = SDL_GetTicks(); clockvirtbase = totalmillis; }
VARFP(clockerror, 990000, 1000000, 1010000, clockreset());
VARFP(clockfix, 0, 0, 1, clockreset());

int getclockmillis()
{
    int millis = SDL_GetTicks() - clockrealbase;
    if(clockfix) millis = int(millis*(double(clockerror)/1000000));
    millis += clockvirtbase;
    return max(millis, totalmillis);
}

VAR(numcpus, 1, 1, 16);

int main(int argc, char **argv)
{
    #ifdef WIN32
    //atexit((void (__cdecl *)(void))_CrtDumpMemoryLeaks);
    #ifndef _DEBUG
    #ifndef __GNUC__
    //__try { Currently broken thanks to syntensity stuff
    #endif
    #endif
    #endif

    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    setlogfile(NULL);

    int dedicated = 0;
    char *load = NULL, *initscript = NULL;

    #define initlog(s) logger::log(logger::INIT, "%s\n", s)

    initing = INIT_RESET;

    /* make sure the path is correct */
    if (!fileexists("data", "r")) {
#ifdef WIN32
        _chdir("..");
#else
        chdir("..");
#endif
    }

    char *loglevel = (char*)"WARNING";
    const char *dir = NULL;
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'q': 
            {
                dir = sethomedir(&argv[i][2]);
                break;
            }
        }
    }
    if (!dir) {
        char *home = getenv("HOME");
        if (home) {
#ifdef WIN32
            defformatstring(defdir)("%s\My Games\OctaForge", home);
#else
            defformatstring(defdir)("%s/.octaforge_client", home);
#endif
            dir = sethomedir(defdir);
        } else {
            dir = sethomedir(".");
        }
    }
    if (dir) {
        logoutf("Using home directory: %s", dir);
    }
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'q': /* parsed first */ break;
            case 'r': /* compat, ignore */ break;
            case 'k':
            {
                const char *dir = addpackagedir(&argv[i][2]);
                if(dir) logoutf("Adding package directory: %s", dir);
                break;
            }
            case 'g': logoutf("Setting logging level %s", &argv[i][2]); loglevel = &argv[i][2]; break;
            case 'd': dedicated = atoi(&argv[i][2]); if(dedicated<=0) dedicated = 2; break;
            case 'w': scr_w = clamp(atoi(&argv[i][2]), SCR_MINW, SCR_MAXW); if(!findarg(argc, argv, "-h")) scr_h = -1; break;
            case 'h': scr_h = clamp(atoi(&argv[i][2]), SCR_MINH, SCR_MAXH); if(!findarg(argc, argv, "-w")) scr_w = -1; break;
            case 'z': /* compat, ignore */ break;
            case 'b': /* compat, ignore */ break;
            case 'a': /* compat, ignore */ break;
            case 'v': vsync = atoi(&argv[i][2]); if(vsync < 0) { vsynctear = 1; vsync = 1; } else vsynctear = 0; break;
            case 't': fullscreen = atoi(&argv[i][2]); break;
            case 's': /* compat, ignore */ break;
            case 'f': /* compat, ignore */ break;
            case 'l': 
            {
                char pkgdir[] = "data/"; 
                load = strstr(path(&argv[i][2]), path(pkgdir)); 
                if(load) load += sizeof(pkgdir)-1; 
                else load = &argv[i][2]; 
                break;
            }
            case 'x': initscript = &argv[i][2]; break;
            default: if(!serveroption(argv[i])) gameargs.add(argv[i]); break;
        }
        else gameargs.add(argv[i]);
    }
    /* Initialize logging at first, right after that lua. */
    logger::setlevel(loglevel);

    initlog("lua");
    lapi::init();
    if (!lapi::state.state()) fatal("cannot initialize lua script engine");
    tools::execcfg("init.lua", true);

    initing = NOT_INITING;

    numcpus = clamp(SDL_GetCPUCount(), 1, 16);

    if(dedicated <= 1)
    {
        initlog("sdl");

        int par = 0;
        #ifdef _DEBUG
        par = SDL_INIT_NOPARACHUTE;
        #endif

        if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO|par)<0) fatal("Unable to initialize SDL: %s", SDL_GetError());
    }

    initlog("net");
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    atexit(enet_deinitialize);
    enet_time_set(0);

    initlog("game");
    game::parseoptions(gameargs);
    initserver(dedicated>0, dedicated>1);  // never returns if dedicated
    ASSERT(dedicated <= 1);
    game::initclient();

    initlog("video: mode");
    setupscreen();

    initlog("video: misc");
    keyrepeat(false);
    SDL_ShowCursor(SDL_FALSE);
    SDL_StopTextInput(); // workaround for spurious text-input events getting sent on first text input toggle?

    initlog("gl");
    gl_checkextensions();
    gl_init(scr_w, scr_h);
    notexture = textureload("data/textures/core/notexture.png");
    if(!notexture) fatal("could not find core textures");

    lapi::state.get<lua::Function>("external", "gl_init")();

    initlog("console");

    types::Tuple<int, const char*> err;

    err = lapi::state.do_file("data/cfg/font.lua");

    if(types::get<0>(err))
        fatal("cannot find font definitions: %s", types::get<1>(err));
    if(!setfont("default")) fatal("no default font specified");

    inbetweenframes = true;
    renderbackground("initializing...");

    initlog("world");
    camera1 = player = game::iterdynents(0);
    //emptymap(0, true, NULL, false);

    initlog("sound");
    initsound();

    initlog("cfg");

    err = lapi::state.do_file("data/cfg/keymap.lua", lua::ERROR_EXIT_TRACEBACK);
    if (types::get<0>(err))
        logger::log(logger::ERROR, "%s\n", types::get<1>(err));
    err = lapi::state.do_file("data/cfg/sounds.lua", lua::ERROR_EXIT_TRACEBACK);
    if (types::get<0>(err))
        logger::log(logger::ERROR, "%s\n", types::get<1>(err));
    err = lapi::state.do_file("data/cfg/menus.lua" , lua::ERROR_TRACEBACK);
    if (types::get<0>(err))
        logger::log(logger::ERROR, "%s\n", types::get<1>(err));
    err = lapi::state.do_file("data/cfg/brush.lua",  lua::ERROR_EXIT_TRACEBACK);
    if (types::get<0>(err))
        logger::log(logger::ERROR, "%s\n", types::get<1>(err));
    err = lapi::state.do_file("mybrushes.lua", lua::ERROR_TRACEBACK);
    if (types::get<0>(err))
        logger::log(logger::ERROR, "%s\n", types::get<1>(err));

    if (game::savedservers())
    {
        err = lapi::state.do_file(
            game::savedservers(), lua::ERROR_TRACEBACK
        );
        if (types::get<0>(err))
            logger::log(logger::ERROR, "%s\n", types::get<1>(err));
    }
    
    identflags |= IDF_PERSIST;
    
    initing = INIT_LOAD;
    if(!tools::execcfg(game::savedconfig())) 
    {
        lapi::state.do_file(game::defaultconfig(), lua::ERROR_EXIT_TRACEBACK);
        tools::writecfg(game::restoreconfig());
    }
    lapi::state.do_file("data/cfg/config.lua", lua::ERROR_EXIT_TRACEBACK);
    err = lapi::state.do_file(game::autoexec(), lua::ERROR_TRACEBACK);
    if (types::get<0>(err))
        logger::log(logger::ERROR, "%s\n", types::get<1>(err));
    initing = NOT_INITING;

    identflags &= ~IDF_PERSIST;

    initing = INIT_GAME;
    game::loadconfigs();
    initing = NOT_INITING;

    initlog("Registering messages\n");
    MessageSystem::MessageManager::registerAll();

    initlog("init: shaders");
    initgbuffer();
    loadshaders();
    particleinit();
    initdecals();

    identflags |= IDF_PERSIST;

    initlog("init: mainloop");

    if(load)
    {
        initlog("localconnect");
        //localconnect();
        game::changemap(load);
    }

    if (initscript)
    {
        err = lapi::state.do_file(initscript, lua::ERROR_TRACEBACK);
        if (types::get<0>(err))
            logger::log(logger::ERROR, "%s\n", types::get<1>(err));
    }

    initmumble();
    resetfpshistory();

    inputgrab(grabinput = true);
    ignoremousemotion();

    for(;;)
    {
        static int frames = 0;
        int millis = getclockmillis();
        limitfps(millis, totalmillis);
        int elapsed = millis-totalmillis;
        if(multiplayer(false)) curtime = game::ispaused() ? 0 : elapsed;
        else
        {
            static int timeerr = 0;
            int scaledtime = elapsed*gamespeed + timeerr;
            curtime = scaledtime/100;
            timeerr = scaledtime%100;
            if(curtime>200) curtime = 200;
            if(paused || game::ispaused()) curtime = 0;
        }
        local_server::try_connect(); /* Try connecting if server is ready */

        lastmillis += curtime;
        totalmillis = millis;

        logger::log(logger::INFO, "New frame: lastmillis: %d   curtime: %d\r\n", lastmillis, curtime); // INTENSITY

        extern void updatetime();
        updatetime();

        checkinput();
        lapi::state.get<lua::Function>("external", "frame_start")();
        tryedit();

        if(lastmillis) game::updateworld();

        serverslice(false, 0);

        if(frames) updatefpshistory(elapsed);
        frames++;

        // miscellaneous general game effects
        recomputecamera();
        updateparticles();
        updatesounds();

        if(minimized) continue;

        if(!gui_mainmenu && ClientSystem::scenarioStarted()) setupframe(screenw, screenh);

        inbetweenframes = false;

        if(gui_mainmenu) gl_drawmainmenu(screenw, screenh);
        else
        {
            // INTENSITY: If we have all the data we need from the server to run the game, then we can actually draw
            if (ClientSystem::scenarioStarted()) gl_drawframe(screenw, screenh);
        }
        swapbuffers();
        renderedframe = inbetweenframes = true;

        ClientSystem::frameTrigger(curtime); // INTENSITY
    }
    
    ASSERT(0);   
    return EXIT_FAILURE;

    #if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    //} __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; } Currently broken thanks to syntensity stuff
    #endif
}
