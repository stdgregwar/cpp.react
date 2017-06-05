#include <iostream>

using namespace std;

#include <SDL/SDL.h>
#include "react/Domain.h"
#include "react/Signal.h"
#include "react/Event.h"
#include "react/Algorithm.h"
#include <unordered_map>

namespace pong {
    struct vec2 {
        float x,y;
        vec2 operator*(float s) const {return vec2{x*s,y*s};}
        vec2 operator+(const vec2& o) const {return vec2{o.x+x,o.y+y};}
        vec2& operator+=(const vec2& o) {
            x+=o.x;
            y+=o.y;
            return *this;
        }
        bool operator==(const vec2& o) const {return o.x == x and o.y == y;}
    };

    struct rect {
        vec2 pos,size;
        bool contains(const vec2& v) const {
            return pos.x < v.x && pos.x+size.x > v.x && pos.y < v.y && pos.y+size.y > v.y;
        }
        operator SDL_Rect() const {
            return SDL_Rect{pos.x,pos.y,size.x,size.y};
        }
        bool operator==(const rect& o) const {
            return tie(pos,size) == tie(o.pos,o.size);
        }
    };

    constexpr int winWidth = 480;
    constexpr int winHeight = 272;
    constexpr float bounce = -1.05f;
    const vec2 barsSize{5,40};
    constexpr float speedFac = 500;
    constexpr float ballRadius = 5;
    const vec2 ballStartSpeed{130,130};

    using KeyMap = unordered_map<SDLKey,vec2>;

    template<class K, class V>
    auto getOrElse(const unordered_map<K,V>& m,const K& key, const V& def) -> const V& {
        auto it = m.find(key);
        if(it != m.end()) return it->second;
        return def;
    }

    template<class V,class W,class X>
    void clamp(V& v, const W& min, const X& max) {
        v = v < min ? min : v > max ? max : v;
    }

    struct MovableState{
        vec2 pos;
        vec2 speed;
        bool operator==(const MovableState& other) const {
            return tie(pos,speed) == tie(other.pos,other.speed);
        }
    };


    auto foldBallState(float dt, MovableState prev, const rect& lbar, const rect& rbar) -> MovableState {
        if(prev.pos.y > winHeight || prev.pos.y < 0) {
            prev.speed.y *= bounce;
            clamp(prev.pos.y,0,winHeight);
        }
        if(prev.pos.x > winWidth || prev.pos.x < 0) {
            //TODO count points
            prev.speed.x*= bounce;
            clamp(prev.pos.x,0,winWidth);
        }
        if(lbar.contains(prev.pos)) prev.speed.x *= bounce;
        if(rbar.contains(prev.pos)) prev.speed.x *= bounce;
        prev.pos += prev.speed*dt;
        return prev;
    }

    auto keysToDir(const KeyMap& m, const SDL_Event& e) -> vec2 {
        float val = e.type == SDL_KEYDOWN ? 1 : -1;
        return getOrElse(m,e.key.keysym.sym,{0,0})*val;
    }


    auto dirToSpeed(const vec2& dir, vec2 speed) -> vec2 {
        return speed += dir*speedFac;
    }

    auto foldBarPos(float dt, vec2 pos, const vec2& speed) -> vec2 {
        auto newPos = pos+speed*dt;
        return newPos;
    }


    auto posToRect(const vec2& size, const vec2& pos) -> rect {
        return rect{pos,size};
    }

    //Player 1 keymap
    KeyMap p1map = {{SDLK_UP,{0,-1}},{SDLK_DOWN,{0,1}},
                    {SDLK_LEFT,{-1,0}},{SDLK_RIGHT,{1,0}}};
    KeyMap p2map = {{SDLK_w,{0,-1}},{SDLK_s,{0,1}},
                    {SDLK_a,{-1,0}},{SDLK_d,{1,0}}};


    using namespace react;
    REACTIVE_DOMAIN(D,sequential)
    USING_REACTIVE_DOMAIN(D)

    //sf::RenderWindow window(sf::VideoMode(winWidth,winHeight),"ReactPong");

    EventSourceT<SDL_Event> eventStream = MakeEventSource<D,SDL_Event>();
    EventSourceT<float>     frameEvent = MakeEventSource<D,float>(); //dt of frames

    EventsT<SDL_Event> keysOnly = Filter(eventStream,[](const SDL_Event& e){return e.type == SDL_KEYDOWN || e.type == SDL_KEYUP;});

    using namespace std::placeholders;
    EventsT<vec2> rmoveDirs = Transform(keysOnly,bind(keysToDir,p1map,_1));
    EventsT<vec2> lmoveDirs = Transform(keysOnly,bind(keysToDir,p2map,_1));

    SignalT<vec2> rbarSpeed = Iterate(rmoveDirs, vec2{0,0}, dirToSpeed);
    SignalT<vec2> rbarPos   = Iterate(frameEvent, vec2{winWidth-20,winHeight/2},With(rbarSpeed), foldBarPos);
    SignalT<rect> rbarRect = MakeSignal(rbarPos,bind(posToRect,barsSize,_1));

    SignalT<vec2> lbarSpeed = Iterate(lmoveDirs, vec2{0,0}, dirToSpeed);
    SignalT<vec2> lbarPos   = Iterate(frameEvent, vec2{20,winHeight/2},With(lbarSpeed), foldBarPos);
    SignalT<rect> lbarRect = MakeSignal(lbarPos,bind(posToRect,barsSize,_1));

    SignalT<MovableState> ballState = Iterate(
                                              frameEvent,
                                              MovableState{{winWidth/2,winHeight/2},ballStartSpeed},
                                              With(lbarRect,rbarRect),
                                              foldBallState
                                              );

    SignalT<rect> ballRect = MakeSignal(MakeSignal(ballState,[](const MovableState& s)->vec2{return s.pos;}),
                                        bind(posToRect,vec2{ballRadius,ballRadius},_1));

    void run() {
        //Close window on sysevent close
        bool quit = false;
        SDL_Rect ball;
        SDL_Rect lbar;
        SDL_Rect rbar;
        Observe(eventStream,[&](const SDL_Event& e) {
                if(e.type == SDL_QUIT) quit = true;
            });

        Observe(ballRect,[&](const rect& r) {
                ball = r;
            });

        Observe(rbarRect,[&](const rect& r) {
                rbar = r;
            });
        Observe(lbarRect,[&](const rect& r) {
                lbar = r;
            });

        SDL_Surface* screen;
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD);
        screen = SDL_SetVideoMode(winWidth,winHeight,16,SDL_SWSURFACE);

        Uint32 color = SDL_MapRGB(screen->format,255,255,255);
        //Event loop
        while(!quit) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                eventStream << event;
            }
            //window.clear();
            SDL_FillRect(screen, NULL, 0x000000);
            frameEvent << 1.f/60; //Fixed time delta
            SDL_FillRect(screen,&ball,color);
            SDL_FillRect(screen,&lbar,color);
            SDL_FillRect(screen,&rbar,color);
            if(SDL_Flip(screen) == -1) break;
            SDL_Delay(1000/60);
        }

        SDL_FreeSurface(screen);
        SDL_Quit();
    }
}

int main(void) {

    pong::run();
    return 0;
}
