#include <map>
using namespace std;

class Order
{
public:
    int order_id;
    float price;
    int quantity;
    bool type_buy;
    Order *next;
    Order *prev;
    Order(int order_id, float price, int quantity, bool buy) : order_id(order_id), price(price), quantity(quantity), type_buy(buy),
        next(nullptr), prev(nullptr) {}
};

class LOB_Naive
{

public:
    map<float, pair<Order *, Order *>, greater<float>> buy_orders;
    map<float, pair<Order *, Order *>> sell_orders;
    map<int, Order *> order_indx;

    void add(int order_id, bool buy, float price, int quantity); // create an order and return order id
    void add_buy(Order *new_order);
    void add_sell(Order *new_oreder);
    void remove_buy_node(Order *curr_order);
    void remove_sell_node(Order *curr_order);
    void reduce(int order_id, int quantity);
    void cancel(int order_id, bool buy, int quantity); // cancels specified amount from an order;
    void execute(int buy_id, int sell_id, int quantity);
};

void LOB_Naive::add_buy(Order *new_order)
{
    auto it = buy_orders.find(new_order->price);
    if (it == buy_orders.end())
    {
        buy_orders[new_order->price] = {new_order, new_order};
    }
    else
    {
        Order *last_tail = (it->second).second;
        last_tail->next = new_order;
        new_order->prev = last_tail;
        (it->second).second = new_order;
    }
}

void LOB_Naive::add_sell(Order *new_order)
{
    auto it = sell_orders.find(new_order->price);
    if (it == sell_orders.end())
    {
        sell_orders[new_order->price] = {new_order, new_order};
    }
    else
    {
        Order *last_tail = (it->second).second;
        last_tail->next = new_order;
        new_order->prev = last_tail;
        (it->second).second = new_order;
    }
}

void LOB_Naive::add(int order_id, bool buy, float price, int quantity)
{

    Order *new_order = new Order(order_id, price, quantity, buy);
    order_indx[order_id] = new_order;
    if (buy)
        add_buy(new_order);
    else
        add_sell(new_order);
}

void LOB_Naive::remove_buy_node(Order *curr_order)
{
    float curr_price = curr_order->price;
    Order *prev = curr_order->prev;
    Order *next = curr_order->next;
    order_indx.erase(curr_order->order_id);
    if (next)
        next->prev = prev;
    else
        buy_orders[curr_price].second = prev;
    if (prev)
        prev->next = next;
    else{
        if(next)
            buy_orders[curr_price].first = next;
        
        else
            buy_orders.erase(curr_price);
    }

    delete (curr_order);
}

void LOB_Naive::remove_sell_node(Order *curr_order)
{
    float curr_price = curr_order->price;
    Order *prev = curr_order->prev;
    Order *next = curr_order->next;
    order_indx.erase(curr_order->order_id);
    if (next)
        next->prev = prev;
    else
        sell_orders[curr_price].second = prev;
    if (prev)
        prev->next = next;
    else{
        if(next)
            sell_orders[curr_price].first = next;
        
        else
            sell_orders.erase(curr_price);
    }
    delete (curr_order);
}

void LOB_Naive::reduce(int order_id, int quantity)
{
    Order *curr_order = order_indx[order_id];
    curr_order->quantity -= quantity;

    if (curr_order->quantity <= 0)
    {
        if (curr_order->type_buy)
        {
            remove_buy_node(curr_order);
        }else{
            remove_sell_node(curr_order);
        }
    }
}

void LOB_Naive::cancel(int order_id, bool buy, int quantity)
{
    reduce(order_id, quantity);
}

void LOB_Naive::execute(int buy_id, int sell_id, int quantity)
{
    reduce(buy_id, quantity);
    reduce(sell_id, quantity);
}

int main()
{
    return 0;
}