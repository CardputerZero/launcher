#include "../main/ui/model/snake_game_model.hpp"

#include <algorithm>
#include <cassert>

int main()
{
    SnakeGameModel model(7);
    assert(model.snake().size() == 3);
    assert((model.snake().front() == SnakeGameModel::Point{20, 9}));
    assert(model.direction() == SnakeGameModel::Direction::RIGHT);
    assert(model.score() == 0);
    assert(std::find(model.snake().begin(), model.snake().end(), model.food()) == model.snake().end());

    model.queue_direction(SnakeGameModel::Direction::LEFT);
    assert(model.tick());
    assert((model.snake().front() == SnakeGameModel::Point{21, 9}));
    assert(model.direction() == SnakeGameModel::Direction::RIGHT);

    model.queue_direction(SnakeGameModel::Direction::UP);
    assert(model.tick());
    assert((model.snake().front() == SnakeGameModel::Point{21, 8}));
    assert(model.direction() == SnakeGameModel::Direction::UP);

    model.reset();
    bool alive = true;
    for (int step = 0; step < 20 && alive; ++step) alive = model.tick();
    assert(!alive);

    model.reset();
    assert((model.snake().front() == SnakeGameModel::Point{20, 9}));
    assert(model.score() == 0);
}
