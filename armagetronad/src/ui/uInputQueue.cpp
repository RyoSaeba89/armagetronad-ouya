/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

***************************************************************************

*/

#include "uInputQueue.h"
#include "rScreen.h"
#include "tConfiguration.h"
#include <iostream>

#ifndef DEDICATED
#include "rSDL.h"
#endif

#ifdef __ANDROID__
#include <android/log.h>
// Per-event pad diagnostics (tag ARMA-INPUT), disabled for release builds —
// they log every axis/button event. To debug controller input, swap the no-op
// for the __android_log_print line and capture with: adb logcat -s ARMA-INPUT
// #define PADLOG(...) __android_log_print( ANDROID_LOG_INFO, "ARMA-INPUT", __VA_ARGS__ )
#define PADLOG(...) ((void)0)
#endif

#include  "tRecorder.h"

#include  "uMenu.h"

static su_TimerCallback *timer=NULL;

su_TimerCallback::su_TimerCallback(){
    timer = this;
}

su_TimerCallback::~su_TimerCallback(){
    if (timer == this)
        timer = NULL;
}

static inline REAL Time(){
    if (timer)
        return timer->GetTime();
    else
        return 0;
}

bool su_prefetchInput=false;
bool su_contInput=true;

#define MAX_PENDING_INPUT 100

static REAL times[MAX_PENDING_INPUT];
static SDL_Event tEvents[MAX_PENDING_INPUT];

static int   currentIn=0,current_out=0,next_in=1;


static inline void increase(int &i){
    i++;
    if (i>=MAX_PENDING_INPUT)
        i=0;
}


static bool input_get=false;

void su_FetchAndStoreSDLInput()
{
#ifndef DEDICATED
#ifndef WIN32
#ifndef MACOSX
    if (!tRecorder::IsRunning() )
        SDL_PumpEvents();
#endif
#endif
#endif
}


bool su_StoreSDLEvent(const SDL_Event &tEvent){
    if (next_in!=current_out && !input_get){
        //con << "Extra input!\n";
        tEvents[currentIn]=tEvent;
        times[currentIn]=Time();
        increase(currentIn);
        next_in=currentIn;
        increase(next_in);
        return false;
    }
    return true;
}

#ifndef DEDICATED
// read and write operators for keysyms
#if SDL_VERSION_ATLEAST(2,0,0)
tRECORDING_ENUM( SDL_Scancode );
tRECORDING_ENUM( SDL_Keymod );
#else
tRECORDING_ENUM( SDLKey );
tRECORDING_ENUM( SDLMod );
#endif
#endif

static char const * recordingSection = "INPUT";

//! Read or write event data
template< class Archiver > class EventArchiver
{
public:
#ifndef DEDICATED
    static void ArchiveKey( Archiver & archive, SDL_KeyboardEvent & key )
    {
        archive.Archive(key.state).Archive(key.keysym.scancode).Archive(key.keysym.sym).Archive(key.keysym.mod)
#if SDL_VERSION_ATLEAST(2,0,0)
        ;
#else
        .Archive(key.keysym.unicode);
#endif
    }
#endif

    static bool Archive( SDL_Event & event, REAL & time, bool & ret )
    {
        // start archive block if archiving is active
        Archiver archive;
        if ( archive.Initialize( recordingSection ) )
        {
#ifndef DEDICATED
            archive.Archive( ret );
            if ( !ret )
                return false;

            // write or read data
            archive.Archive(time).Archive(event.type);
            switch ( event.type )
            {
#if SDL_VERSION_ATLEAST(2,0,0)
            case SDL_WINDOWEVENT:
            {
                SDL_WindowEvent & window = event.window;

                archive.Archive(window.event).Archive(window.data1).Archive(window.data2);
            }
#else
            case SDL_ACTIVEEVENT:
            {
                SDL_ActiveEvent & active = event.active;

                archive.Archive(active.gain).Archive(active.state);
            }
#endif
            break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                SDL_KeyboardEvent & key = event.key;
                ArchiveKey( archive, key );
            }
            break;
            case SDL_MOUSEMOTION:
            {
                SDL_MouseMotionEvent & motion = event.motion;

                archive.Archive(motion.state).Archive(motion.x).Archive(motion.y).Archive(motion.xrel).Archive(motion.yrel);
            }
            break;
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN:
            {
                SDL_MouseButtonEvent & button = event.button;

                archive.Archive(button.button).Archive(button.state).Archive(button.x).Archive(button.y);
            }
            break;
#if SDL_VERSION_ATLEAST(2,0,0)
            case SDL_TEXTINPUT:
            {
                auto &text = event.text.text;

                for(size_t i = 0; i < sizeof(text); ++i)
                {
                    archive.Archive(text[i]);
                    if(!text[i])
                        break;
                }
            }
            break;
#endif
            default:
                // do nothing
                break;
            }

#endif  // DEDICATED

            return true;
        }

        return false;
    }
};

#ifndef DEDICATED
//! Read or write event data
template<>
void EventArchiver< tRecordingBlock >::ArchiveKey( tRecordingBlock & archive, SDL_KeyboardEvent & orig )
{
    SDL_KeyboardEvent key = orig;
    if ( uInputScrambler::Scrambled() )
    {
        switch( key.keysym.sym )
        {
        case SDLK_ESCAPE:
        case SDLK_SPACE:
        case SDLK_KP_ENTER:
        case SDLK_RETURN:
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
        case SDLK_BACKSPACE:
        case SDLK_DELETE:
            break;
        default:
            key.keysym.mod = KMOD_NONE;
            key.keysym.sym = SDLK_x;
#if SDL_VERSION_ATLEAST(2,0,0)
            key.keysym.scancode = SDL_SCANCODE_UNKNOWN;
#else
            key.keysym.scancode = 0;
            key.keysym.unicode = '*';
#endif
        }
    }

    archive.Archive(key.state).Archive(key.keysym.scancode).Archive(key.keysym.sym).Archive(key.keysym.mod)
#if SDL_VERSION_ATLEAST(2,0,0)
        ;
#else
        .Archive(key.keysym.unicode);
#endif
}
#endif

static const char * su_end = "END";
static const char * su_endInput = "ENDINPUT";

// flag indicating input was made and an input start marker is needed for the next input loop
static bool su_markerRequired = false;

void su_EndGetSDLInput()
{
    if ( su_markerRequired )
    {
        // record end of input fetching
        tRecorder::Playback(su_endInput);
        tRecorder::Record(su_endInput);
        su_markerRequired = false;
    }
}

uInputProcessGuard::uInputProcessGuard()
{}
uInputProcessGuard::~uInputProcessGuard()
{
    su_EndGetSDLInput();
}

int uInputScrambler::scrambled_ = 0;

uInputScrambler::uInputScrambler()
{
    scrambled_ ++;
}

uInputScrambler::~uInputScrambler()
{
    --scrambled_;
}

bool uInputScrambler::Scrambled()
{
    return scrambled_ > 0;
}

#ifdef __ANDROID__
// ---------------------------------------------------------------------------
// OUYA controller -> MENU navigation, applied ONLY on the menu input path
// (su_GetMenuInput). The in-game path (su_GetSDLInput, used by gGame) leaves
// joystick events untouched, so they reach the engine's NATIVE bind system
// (config, e.g. "KEYBOARD JOYSTICK_1_HAT_0_LEFT PLAYER_BIND CYCLE_TURN_LEFT 1").
// Nothing is ever pushed onto the SDL queue: conversion is purely in-place, so
// there are no synthetic-event races and the analog stick can never leak into
// steering. This is the deliberate replacement for the old always-on global
// pad->keyboard bridge, which double-bound keys and chattered on the stick.
// ---------------------------------------------------------------------------
static bool su_IsPadEvent( SDL_Event const & e )
{
    switch ( e.type )
    {
    case SDL_JOYAXISMOTION:
    case SDL_JOYBALLMOTION:
    case SDL_JOYHATMOTION:
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        return true;
    default:
        return false;
    }
}
// TEMP diagnostic: log every raw controller event (to logcat via stderr) so the
// exact OUYA layout (hat vs axis, button numbers) can be confirmed. Remove for
// release. Active for both menu and game (called at the central fetch point).
static void su_LogPadEventRaw( SDL_Event const & e )
{
    switch ( e.type )
    {
    case SDL_JOYAXISMOTION:
        PADLOG( "AXIS which=%d num=%d val=%d", (int)e.jaxis.which, (int)e.jaxis.axis, (int)e.jaxis.value );
        break;
    case SDL_JOYHATMOTION:
        PADLOG( "HAT which=%d num=%d val=%d", (int)e.jhat.which, (int)e.jhat.hat, (int)e.jhat.value );
        break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        PADLOG( "BTN which=%d num=%d down=%d", (int)e.jbutton.which, (int)e.jbutton.button,
                ( e.type == SDL_JOYBUTTONDOWN ) );
        break;
    case SDL_JOYBALLMOTION:
        PADLOG( "BALL which=%d num=%d", (int)e.jball.which, (int)e.jball.ball );
        break;
    default:
        break;
    }
}
// The analog stick is intentionally UNBOUND on OUYA: steering is the d-pad
// (buttons 11-14). We drop EVERY controller axis sample so the stick does
// nothing at all - no in-game turn, no menu nav, and the phantom accelerometer
// (instance 1) can't drift anything either. Neutralised events become type 0,
// which su_HandleEvent and su_GetMenuInput both ignore.
static bool su_DropStickAxis( SDL_Event & e )
{
    if ( e.type == SDL_JOYAXISMOTION ) { e.type = 0; return false; }
    return true;
}
static void su_MakeKey( SDL_Event & e, SDL_Scancode sc, SDL_Keycode sym, bool down )
{
    SDL_memset( &e, 0, sizeof( e ) );
    e.type                = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.state           = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.repeat          = 0;
    e.key.keysym.scancode = sc;
    e.key.keysym.sym      = sym;
}
// Convert one axis/hat dimension in place. state holds the last direction
// (-1/0/+1). Returns true if e became a key event, false if nothing to deliver.
static bool su_NavDim( SDL_Event & e, int nd, int & state,
                       SDL_Scancode scN, SDL_Keycode syN, SDL_Scancode scP, SDL_Keycode syP )
{
    if ( nd == state ) return false;                // no transition
    int old = state; state = nd;
    if ( nd == 0 )                                  // released -> key up of old direction
    {
        if ( old < 0 ) { su_MakeKey( e, scN, syN, false ); return true; }
        su_MakeKey( e, scP, syP, false ); return true;
    }
    if ( nd < 0 ) su_MakeKey( e, scN, syN, true );  // pressed a direction -> key down
    else          su_MakeKey( e, scP, syP, true );
    return true;
}
// Map a single pad event to a menu navigation key, in place. Returns true if e
// now holds a usable key event; false if this pad event has no menu meaning and
// should be skipped. State is menu-local (only ever runs on the menu fetch path).
static bool su_PadToMenuKey( SDL_Event & e )
{
    static int mX = 0, mY = 0, mHX = 0, mHY = 0;
    const int TH = 16000;
    switch ( e.type )
    {
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
    {
        if ( e.jbutton.which != 0 ) return false;   // ignore phantom accelerometer
        bool down = ( e.type == SDL_JOYBUTTONDOWN );
        switch ( e.jbutton.button )
        {
        // D-pad arrives as buttons 11-14 on the OUYA controller (no hat) -> menu nav
        case 11: su_MakeKey( e, SDL_SCANCODE_UP,    SDLK_UP,    down ); return true; // d-pad up
        case 12: su_MakeKey( e, SDL_SCANCODE_DOWN,  SDLK_DOWN,  down ); return true; // d-pad down
        case 13: su_MakeKey( e, SDL_SCANCODE_LEFT,  SDLK_LEFT,  down ); return true; // d-pad left
        case 14: su_MakeKey( e, SDL_SCANCODE_RIGHT, SDLK_RIGHT, down ); return true; // d-pad right
        // face buttons
        case 0: su_MakeKey( e, SDL_SCANCODE_RETURN, SDLK_RETURN, down ); return true; // select
        case 3: su_MakeKey( e, SDL_SCANCODE_RETURN, SDLK_RETURN, down ); return true; // select
        case 1: su_MakeKey( e, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, down ); return true; // back
        case 2: su_MakeKey( e, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, down ); return true; // back
        default: return false;
        }
    }
    case SDL_JOYAXISMOTION:
    {
        if ( e.jaxis.which != 0 ) return false;     // ignore phantom accelerometer
        int v = e.jaxis.value;
        int nd = v < -TH ? -1 : ( v > TH ? 1 : 0 );
        if ( e.jaxis.axis == 0 )
            return su_NavDim( e, nd, mX, SDL_SCANCODE_LEFT, SDLK_LEFT, SDL_SCANCODE_RIGHT, SDLK_RIGHT );
        if ( e.jaxis.axis == 1 )
            return su_NavDim( e, nd, mY, SDL_SCANCODE_UP, SDLK_UP, SDL_SCANCODE_DOWN, SDLK_DOWN );
        return false;
    }
    case SDL_JOYHATMOTION:
    {
        int hv = e.jhat.value;
        int nx = ( hv & SDL_HAT_LEFT ) ? -1 : ( hv & SDL_HAT_RIGHT ) ? 1 : 0;
        int ny = ( hv & SDL_HAT_UP )   ? -1 : ( hv & SDL_HAT_DOWN )  ? 1 : 0;
        if ( ny != mHY )                 // vertical transition has priority (list nav)
            return su_NavDim( e, ny, mHY, SDL_SCANCODE_UP, SDLK_UP, SDL_SCANCODE_DOWN, SDLK_DOWN );
        if ( nx != mHX )
            return su_NavDim( e, nx, mHX, SDL_SCANCODE_LEFT, SDLK_LEFT, SDL_SCANCODE_RIGHT, SDLK_RIGHT );
        return false;
    }
    default:
        return false;
    }
}
#endif

bool su_GetSDLInput(SDL_Event &tEvent,REAL &time){
    bool ret=false;

    // clear out data
    memset( &tEvent, 0, sizeof( SDL_Event ) );

    // find end of recording in playback
    if ( tRecorder::Playback(su_end) )
    {
        tRecorder::Record(su_end);
        uMenu::quickexit=uMenu::QuickExit_Total;
    }

    // try to fetch event from playback
    if ( !EventArchiver< tPlaybackBlock >::Archive( tEvent, time, ret ) )
    {
        // get real event
        sr_LockSDL();
        input_get=true;
        if (current_out!=currentIn){
            time=times[current_out];
            tEvent=tEvents[current_out];
            increase(current_out);
            ret=true;
        }
        else{
            time=Time();
            ret=
#ifndef DEDICATED
                SDL_PollEvent(&tEvent);
#else
                false;
#endif
            // NOTE: no pad->keyboard bridge here. In-game, joystick events are
            // delivered RAW so the engine's native bind system handles them.
            // Menu navigation is done separately via su_GetMenuInput().
#ifdef __ANDROID__
            if ( ret && su_IsPadEvent( tEvent ) )
            {
                su_DropStickAxis( tEvent );         // stick unbound: kill all axis samples
                if ( tEvent.type != 0 )
                    su_LogPadEventRaw( tEvent );    // log the remaining (buttons) only
            }
#endif
        }
        sr_UnlockSDL();
        input_get=false;
    }

    su_markerRequired |= ret;

    // store event in recording
    if ( ret )
        EventArchiver< tRecordingBlock >::Archive( tEvent, time, ret );

#ifndef DEDICATED
#if !SDL_VERSION_ATLEAST(2,0,0)
    // filter bogus events. Some keys cause key events with wrong keysyms.
    static unsigned short blockedScancode = 0xffff;
    static SDLKey blockedKeysym = SDLK_LAST;

    if( tEvent.type == SDL_KEYDOWN )
    {
        // you can spot them by zero unicode; control keys are allowed to have that,
        // but not letter and number and sign keys
        if( tEvent.key.keysym.unicode == 0 )
        {
            if ( tEvent.key.keysym.sym >= SDLK_ESCAPE && 
                 tEvent.key.keysym.sym <= SDLK_z )
            {
                ret = false;

                blockedScancode = tEvent.key.keysym.scancode;
                blockedKeysym = tEvent.key.keysym.sym;
            }
        }
    }
    else if ( tEvent.type == SDL_KEYUP )
    {
        if( blockedScancode == tEvent.key.keysym.scancode && 
            blockedKeysym == tEvent.key.keysym.sym )
        {
            ret = false;

            blockedScancode = 0xffff;
            blockedKeysym = SDLK_LAST;
        }
    }
#endif
#endif

    return ret;
}

// Menu input fetch: identical to su_GetSDLInput, except OUYA controller events
// are mapped IN PLACE to menu navigation keys (and pad events with no menu
// meaning are skipped). Used by every menu loop so the whole menu UI is driven
// by the pad without any synthetic events on the SDL queue. The in-game loop
// keeps using su_GetSDLInput (raw joystick -> native binds).
bool su_GetMenuInput( SDL_Event & tEvent, REAL & time )
{
#ifdef __ANDROID__
    for ( ;; )
    {
        if ( !su_GetSDLInput( tEvent, time ) )
            return false;
        if ( tEvent.type == 0 )
            continue;                    // axis sample neutralised by hysteresis: skip
        if ( !su_IsPadEvent( tEvent ) )
            return true;                 // keyboard / mouse: deliver as-is
        // While the on-screen keyboard is up, the controller talks to the IME —
        // but the same physical presses ALSO arrive here as raw joystick events.
        // Converting them would drive the menu behind the keyboard: a d-pad
        // press changes the selected item, the string field gets Deselect()ed,
        // and the keyboard closes instantly ("flashes"). Swallow every pad
        // event while the keyboard is shown; the IME feeds us the typed text.
        if ( sr_screen && SDL_IsScreenKeyboardShown( sr_screen ) )
            continue;
        if ( su_PadToMenuKey( tEvent ) )
            return true;                 // pad mapped to a nav key: deliver it
        // pad event with no menu meaning: skip it and fetch the next one
    }
#else
    return su_GetSDLInput( tEvent, time );
#endif
}

/*
int su_InputThread(void *){
    while (su_contInput){
        if (sr_screen && su_prefetchInput){
            sr_LockSDL();
#ifndef DEDICATED
            SDL_PumpEvents();
#endif
            sr_UnlockSDL();
        }
#ifndef WIN32
        usleep(100000);
#endif
    }
    return 0;
}
*/


