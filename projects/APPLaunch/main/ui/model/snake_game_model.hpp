#pragma once

#include <cstdint>
#include <deque>
#include <random>

class SnakeGameModel
{
public:
    static constexpr int GRID_COLS = 40;
    static constexpr int GRID_ROWS = 18;

    enum class Direction { UP, DOWN, LEFT, RIGHT };

    struct Point {
        int x = 0;
        int y = 0;

        bool operator==(const Point &other) const { return x == other.x && y == other.y; }
    };

    explicit SnakeGameModel(uint32_t seed = 0x534E414B);

    void reset();
    void queue_direction(Direction direction);
    bool tick();

    const std::deque<Point> &snake() const { return snake_; }
    Point food() const { return food_; }
    Direction direction() const { return direction_; }
    int score() const { return score_; }

private:
    bool occupied(Point point) const;
    void spawn_food();

    std::deque<Point> snake_;
    Point food_;
    Direction direction_ = Direction::RIGHT;
    Direction queued_direction_ = Direction::RIGHT;
    int score_ = 0;
    std::minstd_rand random_;
};
