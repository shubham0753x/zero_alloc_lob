#include <vector>
#include <cstdint>

class HierarchicalMask;

class Level{
    std::vector<uint64_t> groups;
    Level* parent;
    Level* child;
    public:
    Level(int n_elements) : parent(nullptr),child(nullptr){
        int size = (n_elements + 63)/64;
        groups.resize(size,0);
        std::fill(groups.begin(),groups.end(),0);
    }

    void activate(int indx){
        int group_indx = indx/64;
        int pos = indx%64;
        bool was_zero = groups[group_indx] == 0;
        groups[group_indx] |= (1ULL<<pos);
        if(was_zero && parent)parent->activate(group_indx);
    }

    void deactivate(int indx){
        int group_indx = indx/64;
        int pos = indx%64;
        groups[group_indx] &= ~(uint64_t{1}<<pos);
        if(groups[group_indx]==0 && parent)parent->deactivate(group_indx);
    }

    friend class HierarchicalMask;
    
};

class HierarchicalMask{
    public:
    Level* root;
    Level* base;

    HierarchicalMask(int n_elements){
        base = new Level(n_elements);
        root = base;
        int curr_size = base->groups.size();
        while(curr_size>1){
            Level* new_root = new Level(curr_size);
            new_root->child = root;
            root->parent = new_root;
            root = new_root;
            curr_size = root->groups.size();
            if(curr_size==1)break;
        }
    }
    // HierarchicalMask()  = default;

    HierarchicalMask(const HierarchicalMask&) = delete;
    HierarchicalMask& operator=(const HierarchicalMask&) = delete;

    ~HierarchicalMask() {
        Level* curr = root;
        while (curr) {
            Level* next = curr->child;
            delete curr;
            curr = next;
        }
    }

    void activate(int indx){
        base->activate(indx);
    }
    void deactivate(int indx){
        base->deactivate(indx);
    }

    int get_right_most_active(){
        if(root->groups[0] == 0)return -1;
        Level* curr = root;
        int parent_indx = 0;

        while(curr){
            parent_indx = 64*parent_indx + 63 -  __builtin_clzll(curr->groups[parent_indx]);
            curr = curr->child;
        }
        return parent_indx;
    }

    int get_left_most_active(){
        if(root->groups[0] == 0)return -1;
        Level* curr = root;
        int parent_indx = 0;

        while(curr){
            parent_indx = 64*parent_indx + __builtin_ctzll(curr->groups[parent_indx]);
            curr = curr->child;
        }
        return parent_indx;
    }

};
