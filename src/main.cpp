#include <iostream>
#include <complex>
#include <stdexcept>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <thread>
#include <sstream>
#include <iomanip>

#include <SFML/Graphics.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Window.hpp>
#include <SFML/Window/ContextSettings.hpp>

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

    void draw(sf::RenderWindow& wnd, Colour col, float mul);

    sf::Vector2f operator()(float t)
    {
        sf::Vector2f r;
        r.x = (1-t)*(1-t)*p0.x+2*(1-t)*t*p1.x+t*t*p2.x;
        r.y = (1-t)*(1-t)*p0.y+2*(1-t)*t*p1.y+t*t*p2.y;
        return r;
    }
};

void QBezier::draw(sf::RenderWindow& wnd, Colour col, float mul)
{
    float len{0};
    auto d = p2-p1;
    len += std::sqrt(d.x*d.x+d.y*d.y);
    d = p1-p0;
    len += std::sqrt(d.x*d.x+d.y*d.y);
    size_t subdiv = std::max<size_t>(8, len*mul*0.95f);
    sf::VertexArray curve(sf::LineStrip, subdiv+1);
    for (size_t i = 0; i <= subdiv; ++i)
    {
        auto t = static_cast<float>(i)/static_cast<float>(subdiv);
        curve[i].position = (*this)(t)*mul;
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
        bezier.p0 = {1, 1};
        bezier.p1 = {4, 4};
        bezier.p2 = {7, 1};
        rayPos = {4, 2};

        if (!font.loadFromFile("font.ttf"))
        {
            throw std::runtime_error("Could not load font.");
        }
    }
    size_t getNearest(sf::Vector2f pos)
    {
        float smallest = std::numeric_limits<float>::max();
        size_t nearest = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            auto d = sqLen(sf::Vector2f{bezier[i].x, bezier[i].y}-pos);
            if (d < smallest)
            {
                nearest = i;
                smallest = d;
            }
        }
        if (sqLen(rayPos - pos) < smallest) return 3;
        return nearest;
    }
    size_t getNearest(sf::Vector2i pos)
    {
        return getNearest(sf::Vector2f{pos.x, pos.y});
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

    size_t accurateIntersectionCount();

    sf::Vector2f pos(size_t i) const
    {
        return (i==3) ? rayPos : sf::Vector2f{bezier[i].x, bezier[i].y};
    }

    void setPos(size_t i, sf::Vector2f pos)
    {
        if (i == 3) rayPos = pos;
        else
        {
            bezier[i] = {pos.x+0.5f, pos.y+0.5f};
        }
    }
    void setPos(size_t i, sf::Vector2i pos)
    {
        if (i == 3) rayPos = {pos.x, pos.y};
        else
        {
            bezier[i] = pos;
        }
    }

    void draw(sf::RenderWindow& wnd, bool isConsistent);

    void setMul(float factor) { mul = factor; }

private:
    sf::Font font;
    QBezier bezier;
    sf::Vector2f rayPos;
    float mul = 10.f;
    void drawBezier(sf::RenderWindow& wnd);
    void drawPoints(sf::RenderWindow& wnd);
    void drawRay(sf::RenderWindow& wnd);
    void drawIntersections(sf::RenderWindow& wnd);
    void drawInfo(sf::RenderWindow& wnd, bool isConsistent);

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
    auto A = pos(0)-pos(1)-pos(1)+pos(2);
    auto B = pos(1)-pos(0);
    B += B;
    auto C = pos(0)-rayPos;
    auto dy = B.y * B.y - 4 * A.y * C.y;
    if (!A.y)
    {
        float t = -(float)C.y/(float)B.y;
        float x = A.x*t*t+B.x*t+C.x;
        if (0 <= x && 0 < t && t < 1)
        {
            intersections.push_back({bezier(t), t});
        }
        return intersections;
    }
    if (dy < 0) return intersections;
    if (!dy)
    {
        float t = -B.y / (float)(2*A.y);
        float x = A.x*t*t+B.x*t+C.x;
        if (0 <= x && 0 < t && t < 1)
        {
            intersections.push_back({bezier(t), t});
        }
        return intersections;
    }
    float rt = std::sqrt((float)dy);
    float t0 = (-B.y+rt) / (float)(2*A.y);
    float x0 = A.x*t0*t0+B.x*t0+C.x;
    if (0 <= x0 && 0 < t0 && t0 < 1)
    {
        intersections.push_back({bezier(t0), t0});
    }
    float t1 = (-B.y-rt) / (float)(2*A.y);
    float x1 = A.x*t1*t1+B.x*t1+C.x;
    if (0 <= x1 && 0 < t1 && t1 < 1)
    {
        intersections.push_back({bezier(t1), t1});
    }
    if (intersections.size() == 2 && x0 < x1)
    {
        std::swap(intersections[0], intersections[1]);
    }
    return intersections;
}

// Note: This uses floating-point arithmetic in some cases since the exact
// expressions are too complicated (and possibly requiring very large numbers).
size_t DrawArea::accurateIntersectionCount()
{
    auto e = bezier.p0.y;
    auto g = bezier.p1.y;
    auto k = bezier.p2.y;
    auto b = rayPos.y;

    auto A = e-2*g+k;
    auto B = 2*(g-e);
    auto C = e-b;

    auto d = bezier.p0.x;
    auto f = bezier.p1.x;
    auto h = bezier.p2.x;
    auto a = rayPos.x;

    auto E = d-2*f+h;
    auto F = 2*(f-d);
    auto G = d-a;

    if (A == 0)
    {
        if (B > 0 && !(b > e && C > -B)) return 0;
        if (!(b < e && C < -B)) return 0;
        float t = -C / (float)B;
        return t * (E * t + F) + G >= 0;
    }

    // Note: This expression may look prone to losing precision, but note that
    // the only non-integer (and thereby non-exact) variable in the expression
    // is b.
    if (e*k-g*g > b*A) return 0;

    bool minusGood = false;
    bool plusGood = false;
    if (e*k-g*g == b*A)
    {
        if (A > 0) plusGood = B < 0 && (-B < 2*A);
        else plusGood = B > 0 && (-B > 2*A);
    }
    else
    {
        // Check that t > 0 for minus solution.
        if (e>g) minusGood = e>b;
        else minusGood = A < 0;

        // Same for plus solution.
        if (e<g) plusGood = e<b;
        else plusGood = A > 0;

        // Check that it still holds when reversing the curve (in effect testing
        // whether t < 1).
        if (plusGood)
        {
            if (k > g) plusGood = k > b;
            else plusGood = A < 0;
        }
        if (minusGood)
        {
            if (k < g) minusGood = k < b;
            else minusGood = A > 0;
        }
    }

    // Just check x using floats since doing it exactly means computing a
    // complicated expression which likely needs 64-bit integers even if all
    // absolute values of the coordinates are less than 2^12. Also, precision is
    // probably not an issue since we only use this to get the x-coordinate of
    // our intersections relative to the ray origin. Yes, it might mean that an
    // intersection very close to the ray origin may be missed (or a non-
    // intersection is counted) but since it is very close to the ray origin, it
    // probably isn't visible.
    float tMinus = (-B - std::sqrt((float)(B*B-4*A*C))) / (float)(2*A);
    float tPlus  = (-B + std::sqrt((float)(B*B-4*A*C))) / (float)(2*A);

    return (tMinus * (E * tMinus + F) + G >= 0) * minusGood
           + (tPlus * (E * tPlus + F) + G >= 0) * plusGood;
}

void DrawArea::drawBezier(sf::RenderWindow& wnd)
{
    sf::VertexArray curveGuide(sf::LineStrip, 3);

    for (size_t i = 0; i < 3; ++i)
    {
        curveGuide[i].position = pos(i) * mul;
        curveGuide[i].color = Colour(0xafafafff);
    }

    wnd.draw(curveGuide);

    bezier.draw(wnd, 0x000000ff, mul);
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
        point.setPosition(pos(i).x*mul-radius, pos(i).y*mul-radius);
        wnd.draw(point);
        text.setString(toString(pos(i)));
        text.setPosition(pos(i).x*mul+radius-text.getLocalBounds().left,
                         pos(i).y*mul+radius-text.getLocalBounds().top);
        wnd.draw(text);
    }
}

void DrawArea::drawRay(sf::RenderWindow& wnd)
{
    sf::VertexArray ray(sf::Lines, 2);
    ray[0].position = pos(3) * mul;
    ray[1].position = sf::Vector2f{wnd.getSize().x, pos(3).y} * mul;
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
        point.setPosition(intersection.point.x*mul-radius,
                           intersection.point.y*mul-radius);
        wnd.draw(point);
        text.setString((std::string)intersection);
        auto placeAt = intersection.point*mul;
        if (placeAbove)
        {
            auto dim = text.getLocalBounds();
            placeAt.x = int(placeAt.x - radius - dim.width - dim.left);
            placeAt.y = int(placeAt.y - radius - dim.height - dim.top);
        }
        else
        {
            placeAt.x = (int)(placeAt.x + radius);
            placeAt.y = (int)(placeAt.y + radius);
        }
        text.setPosition(placeAt);
        wnd.draw(text);
        placeAbove = !placeAbove;
    }
}

void DrawArea::drawInfo(sf::RenderWindow& wnd, bool isConsistent)
{
    struct TextInfo
    {
        std::string text;
        size_t size;
        Colour colour;
    };
    std::vector<TextInfo> texts;
    std::stringstream textBuf;
    if (!isConsistent)
    {
        texts.push_back({"Inconsistent results!!!\n", 24, 0xff0000ff});
    }
    textBuf << "Accurate intersections: " << accurateIntersectionCount() << "\n";
    textBuf << "Approx intersections: " << approxIntersections().size() << "\n";
    texts.push_back({textBuf.str(), 16, 0x000000ff});
    textBuf.str({});
    auto A = pos(0)-pos(1)-pos(1)+pos(2);
    auto B = pos(1)-pos(0);
    B += B;
    auto C = pos(0)-rayPos;
    textBuf << "A: " << toString(A) << "\n";
    textBuf << "B: " << toString(B) << "\n";
    textBuf << "C: " << toString(C) << "\n";
    texts.push_back({textBuf.str(), 12, 0x121212ff});
    textBuf.str({});
    textBuf << "a: " << std::setw(4) << rayPos.x << "      ";
    textBuf << "b: " << std::setw(4) << rayPos.y << "\n";
    textBuf << "d: " << std::setw(4) << pos(0).x << "      ";
    textBuf << "e: " << std::setw(4) << pos(0).y << "\n";
    textBuf << "f: " << std::setw(4) << pos(1).x << "      ";
    textBuf << "g: " << std::setw(4) << pos(1).y << "\n";
    textBuf << "h: " << std::setw(4) << pos(2).x << "      ";
    textBuf << "k: " << std::setw(4) << pos(2).y << "\n";
    texts.push_back({textBuf.str(), 10, 0x303030ff});

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
        dim.x = std::max(dim.x, bounds.width);
        dim.y += bounds.height-bounds.top;
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
        text.setPosition(topleft.x-text.getLocalBounds().left,
                         topleft.y);
        wnd.draw(text);
        topleft.y += text.getLocalBounds().height-text.getLocalBounds().top;
    }
}

void DrawArea::draw(sf::RenderWindow& wnd, bool isConsistent)
{
    auto sz = wnd.getSize();
    wnd.setView(sf::View(sf::FloatRect(0, 0, sz.x, sz.y)));
    wnd.clear(Colour(0xfdfaf1ff));

    drawBezier(wnd);
    drawRay(wnd);
    drawPoints(wnd);
    drawIntersections(wnd);
    drawInfo(wnd, isConsistent);

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

    bool isConsistent = true;

    float mul = 50.f;
    area.setMul(mul);

    while (wnd.isOpen())
    {
        if (isConsistent &&
            area.approxIntersections().size() != area.accurateIntersectionCount())
        {
            isConsistent = false;
            dragging = false;
        }
        if (!isConsistent &&
            area.approxIntersections().size() == area.accurateIntersectionCount())
        {
            isConsistent = true;
        }
        sf::Event evt;
        while (wnd.pollEvent(evt))
        {
            sf::Vector2f mousepos;
            mousepos.x = sf::Mouse::getPosition(wnd).x;
            mousepos.y = sf::Mouse::getPosition(wnd).y;
            mousepos /= mul;
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
                auto nearest = area.getNearest(mousepos);
                auto sqDist = sqLen(area.pos(nearest)-mousepos);
                if (sqDist < (50/mul)*(50/mul))
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
                auto desiredPos = mousepos;
                desiredPos.x = clamp<float>(desiredPos.x, 0, wnd.getSize().x);
                desiredPos.y = clamp<float>(desiredPos.y, 0, wnd.getSize().y);
                area.setPos(dragIdx, desiredPos);
            }
        }
        area.draw(wnd, isConsistent);

    }
}
