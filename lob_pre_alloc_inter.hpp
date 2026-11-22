#pragma once
#include <vector>
#include <new>
#include "pool_allocator.hpp"
#include "absl/container/flat_hash_map.h"
#include <algorithm>
#include <cmath>
#include "heirarchical_bit_mask.hpp"
// #include <unordered_map>
// I need to take the last closed price and percentage it can move in a day and declare an array of the required size that would contain limit objects
// Then i need to reserve memory for a million orders

class Limit;

class Order
{
    int order_id;
    int limit;
    bool type_buy;
    int quantity;
    // Limit *limit_parent;
    Order *next;
    Order *prev;

public:
    Order(int order_id, int limit,bool buy, int quantity) : order_id(order_id), limit(limit), type_buy(buy), quantity(quantity),
                                                         next(nullptr), prev(nullptr) {}

    friend class Limit;
    friend class Book;
};

class Limit
{
    int volume;
    Order *head;
    Order *tail;

public:
    Limit(){
        volume = 0;
        head = nullptr;
        tail = nullptr;
    }

    friend class Book;
};

class Book
{
    std::vector<Limit> buy_orders;
    std::vector<Limit> sell_orders;
    HierarchicalMask best_buy_finder;
    HierarchicalMask best_sell_finder;
    absl::flat_hash_map<int, Order*> order_indx;
    // std:: unordered_map<int,Order*> order_indx;
    PoolAllocator<Order> pool;
    int base_price;
    int tick_size;
    int max_volume;
    
    public:

    int curr_best_buy;
    int curr_best_sell;

    Book(int prev_close_price_cent, int max_change_prcnt, int tick_size, int max_volume)
        : pool(max_volume),
          curr_best_buy(-1),
          curr_best_sell(INT_MAX),
          base_price(static_cast<int>(prev_close_price_cent * (100.0f - max_change_prcnt) / 100.0f)),
          tick_size(tick_size),
          max_volume(max_volume),
          // Calculate size inline to construct masks safely in place
          best_buy_finder(((std::ceil(prev_close_price_cent * (100.0f + max_change_prcnt) / 100.0f) - 
                           static_cast<int>(prev_close_price_cent * (100.0f - max_change_prcnt) / 100.0f)) * tick_size) + 1),
          best_sell_finder(((std::ceil(prev_close_price_cent * (100.0f + max_change_prcnt) / 100.0f) - 
                           static_cast<int>(prev_close_price_cent * (100.0f - max_change_prcnt) / 100.0f)) * tick_size) + 1)
    {
        int max_price = std::ceil(prev_close_price_cent * (100.0f + max_change_prcnt) / 100.0f);
        int size = ((max_price - base_price) * tick_size) + 1;
        
        buy_orders.resize(size);
        sell_orders.resize(size);
        order_indx.reserve(max_volume);
    }

    ~Book(){
        for(auto& pair : order_indx){
            Order* o = pair.second;
            o->~Order();
            pool.deallocate(o);
        }
    }

    int index(int price){
        return (price - base_price)*tick_size;
    }

    bool add(int order_id, int cent_limit, bool buy, int quantity);
    void reduce(int order_id, int quantity);
    void cancel(int order_id, int quantity);
    void execute(int buy_id, int sell_id, int quantity);

    size_t get_map_size() const { 
        return order_indx.size(); 
    }
    size_t get_map_capacity() const { 
        return order_indx.bucket_count(); 
    }
    float get_map_load_factor() const { 
        return order_indx.load_factor(); 
    }
    
};

bool Book::add(int order_id, int  cent_limit, bool buy, int quantity){
    if(cent_limit<base_price)return false;
    int indx = index(cent_limit);
    if(indx>=buy_orders.size())return false;

    Order* new_order = new(pool.allocate()) Order(order_id,cent_limit,buy,quantity);
    order_indx[order_id] = new_order;

    std::vector<Limit>& concerned = buy?buy_orders:sell_orders;

    if(buy){
        if(cent_limit > curr_best_buy)curr_best_buy = cent_limit;
    }else if(cent_limit < curr_best_sell){
        curr_best_sell = cent_limit;
    }

    if(concerned[indx].volume == 0){
        if(buy){
            best_buy_finder.activate(indx);
        }else{
            best_sell_finder.activate(indx);
        }
    }
    concerned[indx].volume += quantity;
    if(!concerned[indx].head){
        concerned[indx].head = new_order;
    }else{
        Order* last_tail = concerned[indx].tail;
        last_tail->next = new_order;
        new_order->prev = last_tail;
    }
    concerned[indx].tail = new_order;

    return true;
    
}

void Book::reduce(int order_id, int quantity){
    auto it = order_indx.find(order_id);
    if(it == order_indx.end())return;
    Order* order = it->second;
    int indx = index(order->limit);
    Limit& parentLimit = order->type_buy?buy_orders[indx]:sell_orders[indx];
    int to_remove = std::min(quantity,order->quantity);
    order->quantity -= to_remove;
    parentLimit.volume -= to_remove;
    if(parentLimit.volume == 0){
        if(order->type_buy){
            best_buy_finder.deactivate(indx);
            if(order->limit == curr_best_buy){
                int new_best_idx = best_buy_finder.get_right_most_active();
                curr_best_buy = (new_best_idx == -1) ? -1 : base_price + (new_best_idx * tick_size);
            }
        }else{
            best_sell_finder.deactivate(indx);
            if(order->limit == curr_best_sell){
              int new_best_idx = best_sell_finder.get_left_most_active();
              curr_best_sell = (new_best_idx == -1) ? INT_MAX : base_price + (new_best_idx * tick_size);
            }
        }
    }
    if(order->quantity == 0){
        
        Order *next = order->next;
        Order *prev = order->prev;
        if(next)next->prev = prev;
        else parentLimit.tail = order->prev;
        if(prev)prev->next = next;
        else parentLimit.head = order->next;
        order->~Order();
        pool.deallocate(order);
        order_indx.erase(it);
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
