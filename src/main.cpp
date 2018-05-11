#include <iostream>
#include <complex>
#include <stdexcept>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <thread>

#include "SFML/Graphics.hpp"
#include "SFML/Graphics/CircleShape.hpp"
#include "SFML/Window.hpp"
#include "SFML/Window/ContextSettings.hpp"

struct Colour
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    Colour() : r{0}, g{0}, b{0}, a{255} {}
    Colour(uint8_t rx, uint8_t gx, uint8_t bx) : r{rx}, g{gx}, b{bx}, a{255} {}
    Colour(uint32_t c) : r((c&0xFF0000)>>16),
                          g((c&0xFF00)>>8),
                          b(c&0xFF),
                          a{255} {}

    operator sf::Color() const { return sf::Color(r, g, b, a); }
};

Colour hsv(float h, float s, float v)
{
    float c = s * v;
    h *= 0.954929659f;
    float x = c * (1.f - std::abs(std::fmod(h, 2.f) - 1.f));
    if (h <= 1.f) return Colour(c * 255, x * 255, 0);
    if (h <= 2.f) return Colour(x * 255, c * 255, 0);
    if (h <= 3.f) return Colour(0, c * 255, x * 255);
    if (h <= 4.f) return Colour(0, x * 255, c * 255);
    if (h <= 5.f) return Colour(x * 255, 0, c * 255);
    if (h <= 6.f) return Colour(c * 255, 0, x * 255);
    return Colour(0, 0, 0);
}

struct QBezier
{
    sf::Vector2i p0, p1, p2;

    void draw(sf::RenderWindow& wnd, Colour col);

    sf::Vector2f operator()(float t)
    {
        sf::Vector2f r;
        r.x = (1-t)*(1-t)*p0.x+2*(1-t)*t*p1.x+t*t*p2.x;
        r.y = (1-t)*(1-t)*p0.y+2*(1-t)*t*p1.y+t*t*p2.y;
        return r;
    }
};

void QBezier::draw(sf::RenderWindow& wnd, Colour col)
{
    float len{0};
    auto d = p2-p1;
    len += std::sqrt(d.x*d.x+d.y*d.y);
    d = p1-p0;
    len += std::sqrt(d.x*d.x+d.y*d.y);
    size_t subdiv = std::max<size_t>(8, len/5);
    sf::VertexArray curve(sf::LineStrip, subdiv+1);
    for (size_t i = 0; i <= subdiv; ++i)
    {
        auto t = static_cast<float>(i)/static_cast<float>(subdiv);
        curve[i].position = (*this)(t);
        curve[i].color = col;
    }
    wnd.draw(curve);
}

void draw(sf::RenderWindow& wnd)
{
    auto sz = wnd.getSize();
    wnd.setView(sf::View(sf::FloatRect(0, 0, sz.x, sz.y)));
    wnd.clear(Colour(0xfdfaf1));

    auto mousepos = sf::Mouse::getPosition(wnd);

    QBezier bcurve;
    bcurve.p0 = {10, 20};
    bcurve.p1 = {532, 846};
    bcurve.p2 = mousepos;
    bcurve.draw(wnd, 0x000000);

    sf::CircleShape cPoint;
    cPoint.setFillColor(Colour(0xffffff));
    cPoint.setOutlineThickness(1.f);
    cPoint.setOutlineColor(Colour(0x000000));
    cPoint.setRadius(3.f);
    cPoint.setPosition(mousepos.x-4, mousepos.y-4);
    wnd.draw(cPoint);

    wnd.display();
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    sf::ContextSettings cs;
    cs.antialiasingLevel = 8;
    sf::RenderWindow wnd(sf::VideoMode(1280, 720),
                         "Bezier curves", sf::Style::Default, cs);
    wnd.setVerticalSyncEnabled(true);

    auto lastSize = wnd.getSize();

    bool needsRender = true;

    bool isDragging = false;
    while (wnd.isOpen())
    {
        sf::Event evt;
        while (wnd.pollEvent(evt))
        {
            if (evt.type == sf::Event::Closed)
            {
                wnd.close();
            }
            if (evt.type == sf::Event::KeyPressed)
            {
                if (evt.key.code == sf::Keyboard::Escape)
                {
                    wnd.close();
                }
            }
            if (evt.type == sf::Event::MouseButtonPressed)
            {
            }
            if (evt.type == sf::Event::MouseMoved
                && sf::Mouse::isButtonPressed(sf::Mouse::Left))
            {
            }
            if (evt.type == sf::Event::MouseButtonReleased
                && evt.mouseButton.button == sf::Mouse::Left)
            {
            }
        }
        draw(wnd);
    }
}
