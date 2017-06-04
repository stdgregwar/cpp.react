#include <iostream>

using namespace std;

#include <SFML/Graphics.hpp>
#include "react/Domain.h"
#include "react/Signal.h"
#include "react/Event.h"
#include "react/Algorithm.h"
#include <unordered_map>

namespace pong {

    constexpr int winWidth = 1280;
    constexpr int winHeight = 720;
    using KeyMap = unordered_map<sf::Keyboard::Key,sf::Vector2f>;
    template<class K, class V>
    auto getOrElse(const unordered_map<K,V>& m,const K& key, const V& def) -> const V& {
        auto it = m.find(key);
        if(it != m.end()) return it->second;
        return def;
    }

    struct MovableState{
        sf::Vector2f pos;
        sf::Vector2f speed;
        bool operator==(const MovableState& other) const {
            return tie(pos,speed) == tie(other.pos,other.speed);
        }
    };

    constexpr float bounce = -1.2f;

    template<class V,class W,class X>
    void clamp(V& v, const W& min, const X& max) {
        v = v < min ? min : v > max ? max : v;
    }

    auto foldBallState(float dt, MovableState prev, const sf::FloatRect& lbar, const sf::FloatRect& rbar) -> MovableState {
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
        prev.pos += dt* prev.speed;
        return prev;
    }

    auto keysToDir(const KeyMap& m, const sf::Event& e) -> sf::Vector2f {
        float val = e.type == sf::Event::KeyPressed ? 1 : -1;
        return getOrElse(m,e.key.code,{0,0})*val;
    }

    constexpr float speedFac = 100;

    auto dirToSpeed(const sf::Vector2f& dir, sf::Vector2f speed) -> sf::Vector2f {
        return speed += dir*speedFac;
    }

    auto foldBarPos(float dt, sf::Vector2f pos, const sf::Vector2f& speed) -> sf::Vector2f {
        auto newPos = pos+dt*speed;
        return newPos;
    }

    const sf::Vector2f barsSize(10,200);

    auto barPosToRect(const sf::Vector2f& pos) -> sf::FloatRect {
        return sf::FloatRect(pos,barsSize);
    }

    //Player 1 keymap
    KeyMap p1map = {{sf::Keyboard::Up,{0,-1}},{sf::Keyboard::Down,{0,1}},
                    {sf::Keyboard::Left,{-1,0}},{sf::Keyboard::Right,{1,0}}};
    KeyMap p2map = {{sf::Keyboard::W,{0,-1}},{sf::Keyboard::S,{0,1}},
                    {sf::Keyboard::A,{-1,0}},{sf::Keyboard::D,{1,0}}};


    using namespace react;
    REACTIVE_DOMAIN(D,sequential)
    USING_REACTIVE_DOMAIN(D)

    sf::RenderWindow window(sf::VideoMode(winWidth,winHeight),"ReactPong");
    sf::RectangleShape lbar(barsSize);
    sf::RectangleShape rbar(barsSize);
    sf::RectangleShape ball({10,10});

    EventSourceT<sf::Event> eventStream = MakeEventSource<D,sf::Event>();
    EventSourceT<float>     frameEvent = MakeEventSource<D,float>(); //dt of frames

    EventsT<sf::Event> keysOnly = Filter(eventStream,[](const sf::Event& e){return e.type == sf::Event::KeyPressed || e.type == sf::Event::KeyReleased;});

    using namespace std::placeholders;
    EventsT<sf::Vector2f> rmoveDirs = Transform(keysOnly,bind(keysToDir,p1map,_1));
    EventsT<sf::Vector2f> lmoveDirs = Transform(keysOnly,bind(keysToDir,p2map,_1));

    SignalT<sf::Vector2f> rbarSpeed = Iterate(rmoveDirs, sf::Vector2f{0,0}, dirToSpeed);
    SignalT<sf::Vector2f> rbarPos   = Iterate(frameEvent, sf::Vector2f{winWidth-20,winHeight/2},With(rbarSpeed), foldBarPos);
    SignalT<sf::FloatRect> rbarRect = MakeSignal(rbarPos,barPosToRect);

    SignalT<sf::Vector2f> lbarSpeed = Iterate(lmoveDirs, sf::Vector2f{0,0}, dirToSpeed);
    SignalT<sf::Vector2f> lbarPos   = Iterate(frameEvent, sf::Vector2f{20,winHeight/2},With(lbarSpeed), foldBarPos);
    SignalT<sf::FloatRect> lbarRect = MakeSignal(lbarPos,barPosToRect);

    SignalT<MovableState> ballState = Iterate(
                                                frameEvent,
                                                MovableState{{winWidth/2,winHeight/2},{130,130}},
                                                With(lbarRect,rbarRect),
                                                foldBallState
                                                );

    void run() {
        //Close window on sysevent close
        Observe(eventStream,[](const sf::Event& e) {
                if(e.type == sf::Event::Closed) window.close();
            });

        Observe(ballState,[](const MovableState& bs) {
                ball.setPosition(bs.pos);
            });

        Observe(rbarPos,[](const sf::Vector2f& pos) {
                rbar.setPosition(pos);
            });
        Observe(lbarPos,[](const sf::Vector2f& pos) {
                lbar.setPosition(pos);
            });


        window.setKeyRepeatEnabled(false);
        window.setFramerateLimit(60);
        //Event loop
        while(window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                eventStream << event;
            }
            window.clear();
            frameEvent << 1.f/60; //Fixed time delta
            window.draw(ball);
            window.draw(lbar);
            window.draw(rbar);
            window.display();
        }
    }
}

int main(void) {

    pong::run();
    return 0;
}
