#include <vector>
#include <unordered_map>





class Chunk{

    public:
        int chunk[65792];
        int pos[2] = {0,0};
        bool read = false;
        bool lcheck = false;
        bool check = false;
        std::vector<int> display_chunk;
};