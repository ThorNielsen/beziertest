#include <iostream>
#include <complex>
#include <stdexcept>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <thread>
#include <sstream>

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
    Colour(uint32_t c) : r((c&0xFF000000)>>24),
                          g((c&0xFF0000)>>16),
                          b((c&0xFF00)>>8),
                          a(c&0xFF) {}

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

    sf::Vector2i& operator[](size_t i)
    {
        switch (i)
        {
        case 0: return p0;
        case 1: return p1;
        case 2: return p2;
        }
    }
    const sf::Vector2i& operator[](size_t i) const
    {
        switch (i)
        {
        case 0: return p0;
        case 1: return p1;
        case 2: return p2;
        }
    }

    void draw(sf::RenderWindow& wnd, Colour col);

    sf::Vector2f operator()(float t)
    {
        sf::Vector2f r;
        r.x = (1-t)*(1-t)*p0.x+2*(1-t)*t*p1.x+t*t*p2.x;
        r.y = (1-t)*(1-t)*p0.y+2*(1-t)*t*p1.y+t*t*p2.y;
        return r;
    }
};

template <typename T>
T sqLen(sf::Vector2<T> v)
{
    return v.x * v.x + v.y * v.y;
}

template <typename T>
std::string toString(sf::Vector2<T> v)
{
    std::stringstream ss;
    ss << "(" << v.x << ", " << v.y << ")";
    return ss.str();
}

struct DrawArea
{
    sf::Font font;
    QBezier bezier;
    size_t getNearest(sf::Vector2i pos)
    {
        int smallest = std::numeric_limits<int>::max();
        size_t nearest = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            auto d = sqLen(bezier[i]-pos);
            if (d < smallest)
            {
                nearest = i;
                smallest = d;
            }
        }
        return nearest;
    }
    sf::Vector2i& pos(size_t i) { return bezier[i]; }
    const sf::Vector2i& pos(size_t i) const { return bezier[i]; }

    void draw(sf::RenderWindow& wnd);
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

void DrawArea::draw(sf::RenderWindow& wnd)
{
    auto sz = wnd.getSize();
    wnd.setView(sf::View(sf::FloatRect(0, 0, sz.x, sz.y)));
    wnd.clear(Colour(0xfdfaf1ff));

    sf::CircleShape cPoint;
    cPoint.setFillColor(Colour(0xffffff00));
    cPoint.setOutlineThickness(1.f);
    cPoint.setOutlineColor(Colour(0x003f1bff));

    float radius = 5;
    cPoint.setRadius(radius);
    sf::VertexArray curveGuide(sf::LineStrip, 3);
    for (size_t i = 0; i < 3; ++i)
    {
        cPoint.setPosition(bezier[i].x-radius, bezier[i].y-radius);
        wnd.draw(cPoint);
        curveGuide[i].position = {bezier[i].x, bezier[i].y};
        curveGuide[i].color = Colour(0xafafafff);
        sf::Text text;
        text.setFont(font);
        text.setString(toString(bezier[i]));
        text.setCharacterSize(12);
        text.setPosition(bezier[i].x+radius, bezier[i].y+radius);
        text.setFillColor(Colour(0x2f5fafff));
        wnd.draw(text);
    }

    wnd.draw(curveGuide);

    bezier.draw(wnd, 0x000000ff);

    wnd.display();
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    sf::RenderWindow wnd(sf::VideoMode(1280, 720), "Bezier curves");
    wnd.setVerticalSyncEnabled(true);

    DrawArea area;
    area.pos(0) = {100, 100};
    area.pos(1) = {400, 400};
    area.pos(2) = {700, 100};

    if (!area.font.loadFromFile("font.ttf"))
    {
        std::cerr << "Error: Could not load font. Exiting.\n";
        return 1;
    }

    bool dragging = false;
    size_t dragIdx = 0;


    while (wnd.isOpen())
    {
        sf::Event evt;
        while (wnd.pollEvent(evt))
        {
            bool shouldUpdateDraggedPosition = dragging;
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
            if (!dragging &&
                evt.type == sf::Event::MouseButtonPressed &&
                evt.mouseButton.button == sf::Mouse::Left)
            {
                auto mousepos = sf::Mouse::getPosition(wnd);
                auto nearest = area.getNearest(mousepos);
                auto sqDist = sqLen(area.pos(nearest)-mousepos);
                if (sqDist < 25*25)
                {
                    dragging = true;
                    dragIdx = nearest;
                    shouldUpdateDraggedPosition = true;
                }
            }

            if (dragging &&
                evt.type == sf::Event::MouseButtonReleased &&
                evt.mouseButton.button == sf::Mouse::Left)
            {
                dragging = false;
                shouldUpdateDraggedPosition = true;
            }

            if (shouldUpdateDraggedPosition)
            {
                area.pos(dragIdx) = sf::Mouse::getPosition(wnd);
            }

        }
        area.draw(wnd);
    }
}
