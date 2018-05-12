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

template <typename T>
inline T clamp(T val, T min, T max)
{
    return std::max(std::min(val, max), min);
}

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

void QBezier::draw(sf::RenderWindow& wnd, Colour col)
{
    float len{0};
    auto d = p2-p1;
    len += std::sqrt(d.x*d.x+d.y*d.y);
    d = p1-p0;
    len += std::sqrt(d.x*d.x+d.y*d.y);
    size_t subdiv = std::max<size_t>(8, len);
    sf::VertexArray curve(sf::LineStrip, subdiv+1);
    for (size_t i = 0; i <= subdiv; ++i)
    {
        auto t = static_cast<float>(i)/static_cast<float>(subdiv);
        curve[i].position = (*this)(t);
        curve[i].color = col;
    }
    wnd.draw(curve);
}

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

class DrawArea
{
public:
    DrawArea()
    {
        pos(0) = {100, 100};
        pos(1) = {400, 400};
        pos(2) = {700, 100};
        pos(3) = {400, 200};

        if (!font.loadFromFile("font.ttf"))
        {
            throw std::runtime_error("Could not load font.");
        }
    }
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
        if (sqLen(rayPos - pos) < smallest) return 3;
        return nearest;
    }

    struct Intersection
    {
        sf::Vector2f point;
        float t;

        operator std::string() const
        {
            std::stringstream text;
            text << "(" << t << ": " << toString(point) << ")";
            return text.str();
        }
    };

    std::vector<Intersection> approxIntersections();

    sf::Vector2i& pos(size_t i) { return (i==3) ? rayPos : bezier[i]; }
    const sf::Vector2i& pos(size_t i) const
    {
        return (i==3) ? rayPos : bezier[i];
    }

    void draw(sf::RenderWindow& wnd);

private:
    sf::Font font;
    QBezier bezier;
    sf::Vector2i rayPos;
    void drawBezier(sf::RenderWindow& wnd);
    void drawPoints(sf::RenderWindow& wnd);
    void drawRay(sf::RenderWindow& wnd);
    void drawIntersections(sf::RenderWindow& wnd);
    void drawInfo(sf::RenderWindow& wnd);

    sf::CircleShape setupPoint(Colour outline, float radius,
                               float thickness = 1.f)
    {
        sf::CircleShape point;
        point.setFillColor(Colour(0xffffff00));
        point.setOutlineThickness(thickness);
        point.setRadius(radius);
        point.setOutlineColor(outline);
        return point;
    }
};

std::vector<DrawArea::Intersection> DrawArea::approxIntersections()
{
    std::vector<Intersection> intersections;
    auto A = pos(0)-2*pos(1)+pos(2);
    auto B = 2*(pos(1)-pos(0));
    auto C = pos(0)-rayPos;
    auto dy = B.y * B.y - 4 * A.y * C.y;
    if (dy < 0) return intersections;
    if (!dy)
    {
        float t = -B.y / (float)(2*A.y);
        float x = A.x*t*t+B.x*t+C.x;
        if (0 <= x && 0 <= t && t <= 1)
        {
            intersections.push_back({bezier(t), t});
        }
        return intersections;
    }
    float rt = std::sqrt((float)dy);
    float t0 = (-B.y+rt) / (float)(2*A.y);
    float x0 = A.x*t0*t0+B.x*t0+C.x;
    if (0 <= x0 && 0 <= t0 && t0 <= 1)
    {
        intersections.push_back({bezier(t0), t0});
    }
    float t1 = (-B.y-rt) / (float)(2*A.y);
    float x1 = A.x*t1*t1+B.x*t1+C.x;
    if (0 <= x1 && 0 <= t1 && t1 <= 1)
    {
        intersections.push_back({bezier(t1), t1});
    }
    if (intersections.size() == 2 && x0 < x1)
    {
        std::swap(intersections[0], intersections[1]);
    }
    return intersections;
}

void DrawArea::drawBezier(sf::RenderWindow& wnd)
{
    sf::VertexArray curveGuide(sf::LineStrip, 3);

    for (size_t i = 0; i < 3; ++i)
    {
        curveGuide[i].position = {pos(i).x, pos(i).y};
        curveGuide[i].color = Colour(0xafafafff);
    }

    wnd.draw(curveGuide);

    bezier.draw(wnd, 0x000000ff);
}

void DrawArea::drawPoints(sf::RenderWindow& wnd)
{
    auto radius = 5.f;
    auto point = setupPoint(0x003f1bff, radius);
    sf::Text text;
    text.setFont(font);
    text.setCharacterSize(12);
    text.setFillColor(Colour(0x2f5fafff));
    for (size_t i = 0; i < 4; ++i)
    {
        point.setPosition(pos(i).x-radius, pos(i).y-radius);
        wnd.draw(point);
        text.setString(toString(pos(i)));
        text.setPosition(pos(i).x+radius, pos(i).y+radius);
        wnd.draw(text);
    }
}

void DrawArea::drawRay(sf::RenderWindow& wnd)
{
    sf::VertexArray ray(sf::Lines, 2);
    ray[0].position = {pos(3).x, pos(3).y};
    ray[1].position = {wnd.getSize().x, pos(3).y};
    ray[0].color = ray[1].color = Colour(0xff6c1dff);
    wnd.draw(ray);
}

void DrawArea::drawIntersections(sf::RenderWindow& wnd)
{
    sf::Text text;
    text.setFont(font);
    text.setCharacterSize(12);
    text.setFillColor(Colour(0x3212afff));
    float radius = 3.f;
    auto point = setupPoint(0xff00ffff, radius);
    bool placeAbove = true;
    for (auto& intersection : approxIntersections())
    {
        point.setPosition(intersection.point.x-radius,
                           intersection.point.y-radius);
        wnd.draw(point);
        text.setString((std::string)intersection);
        if (placeAbove)
        {
            auto dim = text.getLocalBounds();
            text.setPosition((int)(intersection.point.x-radius-dim.width),
                             (int)(intersection.point.y-radius-dim.height));
        }
        else
        {
            text.setPosition((int)(intersection.point.x+radius+0.5),
                             (int)(intersection.point.y+radius+0.5));
        }
        wnd.draw(text);
        placeAbove = !placeAbove;
    }
}

void DrawArea::drawInfo(sf::RenderWindow& wnd)
{
    struct TextInfo
    {
        std::string text;
        size_t size;
        Colour colour;
    };
    std::vector<TextInfo> texts;
    std::stringstream textBuf;
    textBuf << "Exact intersections: " << " unimplemented." << "\n";
    textBuf << "Approx intersections: " << approxIntersections().size() << "\n";
    texts.push_back({textBuf.str(), 16, 0x000000ff});
    textBuf.str({});
    auto A = pos(0)-2*pos(1)+pos(2);
    auto B = 2*(pos(1)-pos(0));
    auto C = pos(0)-rayPos;
    textBuf << "A: " << toString(A) << "\n";
    textBuf << "B: " << toString(B) << "\n";
    textBuf << "C: " << toString(C) << "\n";
    texts.push_back({textBuf.str(), 12, 0x121212ff});
    textBuf.clear();

    // Trim last newline of last text in order to fit background rectangle to
    // actual text area.
    if (texts.back().text.size() && texts.back().text.back() == '\n')
    {
        texts.back().text.pop_back();
    }

    std::vector<sf::Text> renderTexts;
    sf::Vector2f dim{0, 0};
    for (auto& info : texts)
    {
        renderTexts.push_back({});
        sf::Text& text = renderTexts.back();
        text.setFont(font);
        text.setCharacterSize(info.size);
        text.setFillColor(info.colour);
        text.setString(info.text);
        auto bounds = text.getLocalBounds();
        dim.x = std::max(dim.x, bounds.width - bounds.left);
        dim.y += bounds.height + bounds.top;
    }
    sf::Vector2f topleft;
    topleft.x = wnd.getSize().x-dim.x;
    topleft.y = 0;
    sf::RectangleShape bg;
    bg.setPosition(topleft);
    bg.setSize(dim);
    bg.setFillColor(Colour(0x0000001a));
    wnd.draw(bg);
    for (auto& text : renderTexts)
    {
        text.setPosition(topleft);
        wnd.draw(text);
        topleft.y += text.getLocalBounds().height;
    }
}

void DrawArea::draw(sf::RenderWindow& wnd)
{
    auto sz = wnd.getSize();
    wnd.setView(sf::View(sf::FloatRect(0, 0, sz.x, sz.y)));
    wnd.clear(Colour(0xfdfaf1ff));

    drawBezier(wnd);
    drawRay(wnd);
    drawPoints(wnd);
    drawIntersections(wnd);
    drawInfo(wnd);

    wnd.display();
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    sf::RenderWindow wnd(sf::VideoMode(1280, 720), "Bezier curves");
    wnd.setVerticalSyncEnabled(true);

    DrawArea area;

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
                auto desiredPos = sf::Mouse::getPosition(wnd);
                desiredPos.x = clamp<int>(desiredPos.x, 0, wnd.getSize().x);
                desiredPos.y = clamp<int>(desiredPos.y, 0, wnd.getSize().y);
                area.pos(dragIdx) = desiredPos;
            }
        }
        area.draw(wnd);

    }
}
