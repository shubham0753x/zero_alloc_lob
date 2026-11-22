#include <map>
#include <unordered_map>
using namespace std;

class Limit;

class Order
{
    int order_id;
    int limit;
    bool type_buy;
    int quantity;
    Limit *limit_parent;
    Order *next;
    Order *prev;

public:
    Order(int order_id, int limit, bool buy, int quantity) : order_id(order_id), limit(limit), type_buy(buy), quantity(quantity),
                                                             limit_parent(nullptr), next(nullptr), prev(nullptr) {}

    friend class Limit;
    friend class Book;
};

class Limit
{
    int limit;
    int volume;
    Limit *next;
    Limit *prev;
    Order *head;
    Order *tail;

public:
    Limit(Order *order)
    {
        limit = order->limit;
        volume = order->quantity;
        head = order;
        tail = order;
    }

    friend class Book;
};

class Book
{
    map<int, Limit *, greater<int>> buy_orders;
    map<int, Limit *> sell_orders;
    unordered_map<int, Limit *> buy_limit_indx;
    unordered_map<int, Limit *> sell_limit_indx;
    unordered_map<int, Order *> order_indx;

public:
    void add(int order_id, int limit, bool buy, int quantity);
    void reduce(int order_id, int quantity);
    void cancel(int order_id, int quantity);
    void execute(int buy_id, int sell_id, int quantity);

    ~Book()
    {
        for (auto &pair : order_indx){
            Order* o = pair.second;
            delete o;
        }
        for (auto &pair : buy_orders){
            Limit* l = pair.second;
            delete l;
        }
        for (auto &pair : sell_orders){
            Limit* l = pair.second;
            delete l;
        }
    }
};

void Book::add(int order_id, int limit, bool buy, int quantity)
{
    Order *new_order = new Order(order_id, limit, buy, quantity);
    order_indx[order_id] = new_order;
    int curr_limit = new_order->limit;
    auto it = buy ? buy_limit_indx.find(curr_limit) : sell_limit_indx.find(curr_limit);
    Limit *limit_node;
    if ((buy && it == buy_limit_indx.end()) || (!buy && it == sell_limit_indx.end()))
    {
        limit_node = new Limit(new_order);
        buy ? buy_limit_indx[curr_limit] = limit_node : sell_limit_indx[curr_limit] = limit_node;
        buy ? buy_orders[curr_limit] = limit_node : sell_orders[curr_limit] = limit_node;
    }
    else
    {
        limit_node = buy ? buy_limit_indx[curr_limit] : sell_limit_indx[curr_limit];
        Order *curr_tail = limit_node->tail;
        curr_tail->next = new_order;
        new_order->prev = curr_tail;
        limit_node->volume += new_order->quantity;
    }
    new_order->limit_parent = limit_node;
    limit_node->tail = new_order;
}

void Book::reduce(int order_id, int quantity)
{
    if (order_indx.find(order_id) == order_indx.end())
        return;
    Order *order = order_indx[order_id];
    int available = order->quantity;
    order->quantity -= quantity;
    Limit *parent_limit = order->limit_parent;
    parent_limit->volume -= min(available, quantity);

    if (order->quantity <= 0)
    {
        Order *next = order->next;
        Order *prev = order->prev;
        order_indx.erase(order->order_id);

        if (next)
            next->prev = prev;
        else
            parent_limit->tail = prev;

        if (prev)
            prev->next = next;
        else
        {
            if (next)
                parent_limit->head = next;
            else
            {
                if (order->type_buy)
                    buy_orders.erase(order->limit);
                else
                    sell_orders.erase(order->limit);
                order->type_buy ? buy_limit_indx.erase(order->limit) : sell_limit_indx.erase(order->limit);
                delete (parent_limit);
            }
        }

        delete (order);
    }
}

void Book::cancel(int order_id, int quantity)
{
    reduce(order_id, quantity);
}

void Book::execute(int buy_id, int sell_id, int quantity)
{
    reduce(buy_id, quantity);
    reduce(sell_id, quantity);
}

int main()
{
    return 0;
}