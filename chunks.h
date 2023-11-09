#include "chunk.h"
#include "lightchunk.h"
#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <cmath>
#include <mutex>
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <functional>

struct ChunkPos // 储存区块坐标的自定义数据类型
{
    int x, y;
};
bool operator==(const ChunkPos &lhs, const ChunkPos &rhs)
{
    return (lhs.x == rhs.x) && (lhs.y == rhs.y);
}
struct ChunkPosHash // 用与给ChunkPos数据生成哈希值
{
    std::size_t operator()(const ChunkPos &cp) const
    {
        // 使用位运算将 x 和 y 合并成一个哈希值
        return std::hash<int>()(cp.x) ^ std::hash<int>()(cp.y);
    }
};
struct ChunkOP // 储存区块中方块的索引数据的自定义数据类型
{
    ChunkPos pos;
    int index;
};
bool operator==(const ChunkOP &lhs, const ChunkOP &rhs)
{
    return (lhs.pos == rhs.pos) && (lhs.index == rhs.index);
}
class Chunks
{
public:
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks;                                     // 区块信息（方块信息，是否创建了地形，是否确认了要显示的方块的面，所有要显示的方块信息）
    std::unordered_map<ChunkPos, LightChunk, ChunkPosHash> lightchunks;                           // 区块中的亮度等级
    std::unordered_map<ChunkPos, std::unordered_map<int, std::vector<int>>, ChunkPosHash> lights; // 区块中的光源
    int toffset[2] = {-12, -12};                                                                  // 用于对显示区块的范围的偏移坐标的缓冲
    int offset[2] = {-12, -12};                                                                   // 显示区块的范围的偏移坐标
    std::unordered_map<int, int> linetomap;                                                       // 记录外围区块到中心区块的距离
    std::mutex mapMutex;
    int linetmap[529];                                   // 用于给从中心区块到外围区块排序
    bool gamerun = true;                                 // 游戏是否正在运行
    bool recheckthreadover = false;                      // 光照加载线程是否结束
    bool readthreadover = false;                         // 区块加载线程是否结束
    void cltm();                                         // 给从中心区块到外围区块排序
    void load();                                         // 启动线程与初始化
    void reading();                                      // 区块加载
    void rechecking();                                   // 光照加载
    void read(int n);                                    // 地形生成
    void check(int n);                                   // 计算方块表面亮度（包括平滑光照）与要显示的面
    ChunkPos offset_cPos(ChunkPos cPos, int ox, int oy); // 对输入的区块坐标进行偏移并返回
    ChunkOP Get_BlockCO(int x, int y, int z);            // 将输入坐标转为可适用于程序内使用的索引数据
    void light_check(int cpos);                          // 计算区块中的亮度等级
    // perlin噪声部分
    double *move_array(double p[2], double a, double b);
    double dot(double a[2], double b[2]);
    double fade(double x);
    double mix(double x, double y, double z);
    double perlin(double pos[2], double k[2], int key);
    double *random(double k[2], int key);

    void flood_fill_light(int x, int y, int z, int color); // 原先用于测试光照等级扩散的深度优先算法
};

ChunkOP Chunks::Get_BlockCO(int x, int y, int z)
{
    ChunkOP BlockData = {{static_cast<int>(floor(x / 16.0)), static_cast<int>(floor(z / 16.0))}, x - 16 * static_cast<int>(floor(x / 16.0)) + y * 256 + z * 16 - 256 * static_cast<int>(floor(z / 16.0))};
    return BlockData;
}

void Chunks::cltm()
{
    for (int i = 0; i < 529; i++)
    {

        linetomap[i] = (i % 23 - 12) * (i % 23 - 12) + (i / 23 - 12) * (i / 23 - 12);
    }
    int t = 0;
    for (int i = 0; i < 529; i++)
    {
        auto minPair = linetomap.begin();
        for (auto it = linetomap.begin(); it != linetomap.end(); ++it)
        {
            if (it->second < minPair->second)
            {
                minPair = it;
            }
        }

        linetmap[t] = minPair->first;
        linetomap.erase(minPair);
        t++;
    }
}

ChunkPos Chunks::offset_cPos(ChunkPos cPos, int ox, int oy)
{
    ChunkPos rpos;
    rpos.x = cPos.x + ox;
    rpos.y = cPos.y + oy;
    return rpos;
}

void Chunks::read(int n)
{
    int cx = n % 23 + offset[0], cy = n / 23 + offset[1];
    std::string str_x = std::to_string(cx);
    std::string str_y = std::to_string(cy);
    std::string str = str_x + " " + str_y;
    chunks[{cx, cy}].pos[0] = n % 23 * 1.0 + offset[0];
    chunks[{cx, cy}].pos[1] = n / 23 * 1.0 + offset[1];
    std::ifstream chunk_save;
    std::stringstream b;
    std::string sn = std::to_string(n);
    chunk_save.open("saves/1/" + str + ".data", std::ios::in);
    std::string a;
    b << chunk_save.rdbuf();
    a = b.str();
    chunk_save.close();
    // std::cout << a << "" << std::endl;
    if (true) // if (a == "")
    {
        double perlinh[256];
        double CPos[] = {n % 23 * 1.0 + offset[0], n / 23 * 1.0 + offset[1]}; // 区块坐标

        for (int i = 0; i < 256; i++)
        {
            double OPos[] = {i % 16 / 16.0, static_cast<int>(i / 16) / 16.0}; // 在区块中的相对方块坐标（x，z）

            perlinh[i] = perlin(CPos, OPos, 99547947) + 1.0; // 地形噪声
        }
        for (int i = 0; i < 65536; i++)
        {
            int OPos[3] = {i % 16, i / 256, i % 256 / 16}; // 在区块中的相对方块坐标（x,y,z）

            double h = perlinh[i % 256]; //*0;

            if (OPos[0] == 0 && OPos[2] == 0 && CPos[0] == 0 && CPos[1] == 0) // 用来确定哪些地方要放萤石（光源）
            {
                chunks[{cx, cy}].chunk[i] = 4; // 萤石的方块ID为4
                lightchunks[{cx, cy}].lights[i] = 15;
                lights[{cx, cy}][15].push_back(i);
            }
            else
            {
                if (i / 256 > 50 + static_cast<int>(h))
                {
                    chunks[{cx, cy}].chunk[i] = 0; // 空气
                }
                else if (i / 256 <= 45 + static_cast<int>(h))
                {
                    chunks[{cx, cy}].chunk[i] = 1; // 石头
                }
                else if (i / 256 < 50 + static_cast<int>(h))
                {
                    chunks[{cx, cy}].chunk[i] = 3; // 泥土
                }
                else if (i / 256 == 50 + static_cast<int>(h))
                {
                    chunks[{cx, cy}].chunk[i] = 2; // 草方块
                }
            }
        }
        std::ofstream chunk_save;
        /*chunk_save.open("saves/1/" + str + ".data", std::ios::out);
        for (int i = 0; i < 65536; i++)
        {
            chunk_save << chunks[str].chunk[i] << " ";
        }*/
        chunk_save.close();
    }
    else
    {
        std::fstream chunk_save;
        chunk_save.open("saves/1/" + str + ".data", std::ios::in);
        int temp;
        for (int i = 0; i < 65536; i++)
        {
            chunk_save >> temp;
            chunks[{cx, cy}].chunk[i] = temp;
        }
    }
    chunks[{cx, cy}].read = true;
}

void Chunks::check(int n)
{
    int cx, cy, temp;
    int pow16[6] = {1, 16, 256, 4096, 65536, 1048576}; // 16的次方
    cx = n % 21 + 1 + offset[0];
    cy = n / 21 + 1 + offset[1];
    ChunkPos cpos0 = {cx, cy};
    ChunkPos cpos1 = {cx + 1, cy};
    ChunkPos cpos2 = {cx, cy + 1};
    ChunkPos cpos3 = {cx - 1, cy};
    ChunkPos cpos4 = {cx, cy - 1};
    ChunkPos cpos5 = {cx + 1, cy + 1};
    ChunkPos cpos6 = {cx - 1, cy + 1};
    ChunkPos cpos7 = {cx - 1, cy - 1};
    ChunkPos cpos8 = {cx + 1, cy - 1};

    for (int i = 0; i < 65536; i++)
    {
        if (chunks[cpos0].chunk[i] != 0)
        {
            int g = 0;
            int OPos[3] = {i % 16, i / 256, i % 256 / 16};
            if (OPos[2] >= 1)
            {
                if (chunks[cpos0].chunk[(i - 16)] != 0)
                {
                    g += 1;
                }
            }
            else
            {
                if (chunks[cpos4].chunk[(i + 240)] != 0)
                {
                    g += 1;
                }
            }
            if (OPos[2] <= 14)
            {
                if (chunks[cpos0].chunk[(i + 16)] != 0)
                {
                    g += 2;
                }
            }
            else
            {
                if (chunks[cpos2].chunk[(i - 240)] != 0)
                {
                    g += 2;
                }
            }
            if (OPos[0] >= 1)
            {
                if (chunks[cpos0].chunk[(i - 1)] != 0)
                {
                    g += 4;
                }
            }
            else
            {
                if (chunks[cpos3].chunk[(i + 15)] != 0)
                {
                    g += 4;
                }
            }
            if (OPos[0] <= 14)
            {
                if (chunks[cpos0].chunk[(i + 1)] != 0)
                {
                    g += 8;
                }
            }
            else
            {
                if (chunks[cpos1].chunk[(i - 15)] != 0)
                {
                    g += 8;
                }
            }
            if (OPos[1] >= 1)
            {
                if (chunks[cpos0].chunk[(i - 256)] != 0)
                {
                    g += 16;
                }
            }
            if (OPos[1] <= 255)
            {
                if (chunks[cpos0].chunk[(i + 256)] != 0)
                {
                    g += 32;
                }
            }
            if (g != 63)
            {
                int center_light[6] = {}; // 方块表面的亮度
                int corner_light[8] = {}; // 方块表面的位置的斜角的亮度
                int corner_block[8] = {}; // 方块表面的位置的斜角的方块
                int side_light[12] = {};  // 方块方块表面的位置的边的亮度
                int side_block[12] = {};  // 方块方块表面的位置的边的方块
                if (OPos[2] >= 1)         // 检查是否位于区块的前方的最外层
                {
                    center_light[0] = lightchunks[cpos0].lights[(i - 16)];
                    if (OPos[1] <= 255) // 检查是否位于区块的上方的最最高层
                    {
                        side_light[0] = lightchunks[cpos0].lights[(i + 240)];
                        side_block[0] = chunks[cpos0].chunk[(i + 240)];
                        if (OPos[0] >= 1) // 检查是否位于区块的左方的最外层
                        {
                            corner_light[0] = lightchunks[cpos0].lights[(i + 239)];
                            corner_block[0] = chunks[cpos0].chunk[(i + 239)];
                        }
                        else
                        {
                            corner_light[0] = lightchunks[cpos3].lights[(i + 255)];
                            corner_block[0] = chunks[cpos3].chunk[(i + 255)];
                        }
                        if (OPos[0] <= 14) // 检查是否位于区块的右方的最外层
                        {
                            corner_light[1] = lightchunks[cpos0].lights[(i + 241)];
                            corner_block[1] = chunks[cpos0].chunk[(i + 241)];
                        }
                        else
                        {
                            corner_light[1] = lightchunks[cpos1].lights[(i + 225)];
                            corner_block[1] = chunks[cpos1].chunk[(i + 225)];
                        }
                    }
                    if (OPos[1] >= 1) // 检查是否位于区块的下方的最低层
                    {
                        side_light[1] = lightchunks[cpos0].lights[(i - 272)];
                        side_block[1] = chunks[cpos0].chunk[(i - 272)];

                        if (OPos[0] >= 1)
                        {
                            corner_light[2] = lightchunks[cpos0].lights[(i - 273)];
                            corner_block[2] = chunks[cpos0].chunk[(i - 273)];
                        }
                        else
                        {
                            corner_light[2] = lightchunks[cpos3].lights[(i - 257)];
                            corner_block[2] = chunks[cpos3].chunk[(i - 257)];
                        }
                        if (OPos[0] <= 14)
                        {
                            corner_light[3] = lightchunks[cpos0].lights[(i - 271)];
                            corner_block[3] = chunks[cpos0].chunk[(i - 271)];
                        }
                        else
                        {
                            corner_light[3] = lightchunks[cpos1].lights[(i - 287)];
                            corner_block[3] = chunks[cpos1].chunk[(i - 287)];
                        }
                    }
                    if (OPos[0] >= 1) // 检查是否位于区块的前方的最外层
                    {
                        side_light[2] = lightchunks[cpos0].lights[(i - 17)];
                        side_block[2] = chunks[cpos0].chunk[(i - 17)];
                    }
                    else
                    {
                        side_light[2] = lightchunks[cpos3].lights[(i - 1)];
                        side_block[2] = chunks[cpos3].chunk[(i - 1)];
                    }
                    if (OPos[0] <= 14)
                    {
                        side_light[3] = lightchunks[cpos0].lights[(i - 15)];
                        side_block[3] = chunks[cpos0].chunk[(i - 15)];
                    }
                    else
                    {
                        side_light[3] = lightchunks[cpos1].lights[(i - 31)];
                        side_block[3] = chunks[cpos1].chunk[(i - 31)];
                    }
                }
                else
                {
                    center_light[0] = lightchunks[cpos4].lights[(i + 240)];
                    if (OPos[1] <= 255)
                    {
                        side_light[0] = lightchunks[cpos4].lights[(i + 496)];
                        side_block[0] = chunks[cpos4].chunk[(i + 496)];
                        if (OPos[0] >= 1)
                        {
                            corner_light[0] = lightchunks[cpos4].lights[(i + 495)];
                            corner_block[0] = chunks[cpos4].chunk[(i + 495)];
                        }
                        else
                        {
                            corner_light[0] = lightchunks[cpos7].lights[(i + 511)];
                            corner_block[0] = chunks[cpos7].chunk[(i + 511)];
                        }
                        if (OPos[0] <= 14)
                        {
                            corner_light[1] = lightchunks[cpos4].lights[(i + 497)];
                            corner_block[1] = chunks[cpos4].chunk[(i + 497)];
                        }
                        else
                        {
                            corner_light[1] = lightchunks[cpos8].lights[(i + 481)];
                            corner_block[1] = chunks[cpos8].chunk[(i + 481)];
                        }
                    }
                    if (OPos[1] >= 1)
                    {
                        side_light[1] = lightchunks[cpos4].lights[(i - 16)];
                        side_block[1] = chunks[cpos4].chunk[(i - 16)];

                        if (OPos[0] >= 1)
                        {
                            corner_light[2] = lightchunks[cpos4].lights[(i - 17)];
                            corner_block[2] = chunks[cpos4].chunk[(i - 17)];
                        }
                        else
                        {
                            corner_light[2] = lightchunks[cpos7].lights[(i - 1)];
                            corner_block[2] = chunks[cpos7].chunk[(i - 1)];
                        }
                        if (OPos[0] <= 14)
                        {
                            corner_light[3] = lightchunks[cpos4].lights[(i - 15)];
                            corner_block[3] = chunks[cpos4].chunk[(i - 15)];
                        }
                        else
                        {
                            corner_light[3] = lightchunks[cpos8].lights[(i - 31)];
                            corner_block[3] = chunks[cpos8].chunk[(i - 31)];
                        }
                    }
                    if (OPos[0] >= 1)
                    {
                        side_light[2] = lightchunks[cpos4].lights[(i + 239)];
                        side_block[2] = chunks[cpos4].chunk[(i + 239)];
                    }
                    else
                    {
                        side_light[2] = lightchunks[cpos7].lights[(i + 255)];
                        side_block[2] = chunks[cpos7].chunk[(i + 255)];
                    }

                    if (OPos[0] <= 14)
                    {
                        side_light[3] = lightchunks[cpos4].lights[(i + 241)];
                        side_block[3] = chunks[cpos4].chunk[(i + 241)];
                    }
                    else
                    {
                        side_light[3] = lightchunks[cpos8].lights[(i + 225)];
                        side_block[3] = chunks[cpos8].chunk[(i + 225)];
                    }
                }
                if (OPos[2] <= 14) // 检查是否位于区块的后方方的最外层
                {
                    center_light[1] = lightchunks[cpos0].lights[(i + 16)];
                    if (OPos[1] <= 255)
                    {
                        side_light[4] = lightchunks[cpos0].lights[(i + 272)];
                        side_block[4] = chunks[cpos0].chunk[(i + 272)];
                        if (OPos[0] >= 1)
                        {
                            corner_light[4] = lightchunks[cpos0].lights[(i + 271)];
                            corner_block[4] = chunks[cpos0].chunk[(i + 271)];
                        }
                        else
                        {
                            corner_light[4] = lightchunks[cpos3].lights[(i + 287)];
                            corner_block[4] = chunks[cpos3].chunk[(i + 287)];
                        }
                        if (OPos[0] <= 14)
                        {
                            corner_light[5] = lightchunks[cpos0].lights[(i + 273)];
                            corner_block[5] = chunks[cpos0].chunk[(i + 273)];
                        }
                        else
                        {
                            corner_light[5] = lightchunks[cpos1].lights[(i + 257)];
                            corner_block[5] = chunks[cpos1].chunk[(i + 257)];
                        }
                    }
                    if (OPos[1] >= 1)
                    {
                        side_light[5] = lightchunks[cpos0].lights[(i - 240)];
                        side_block[5] = chunks[cpos0].chunk[(i - 240)];

                        if (OPos[0] >= 1)
                        {
                            corner_light[6] = lightchunks[cpos0].lights[(i - 241)];
                            corner_block[6] = chunks[cpos0].chunk[(i - 241)];
                        }
                        else
                        {
                            corner_light[6] = lightchunks[cpos3].lights[(i - 225)];
                            corner_block[6] = chunks[cpos3].chunk[(i - 225)];
                        }
                        if (OPos[0] <= 14)
                        {
                            corner_light[7] = lightchunks[cpos0].lights[(i - 239)];
                            corner_block[7] = chunks[cpos0].chunk[(i - 239)];
                        }
                        else
                        {
                            corner_light[7] = lightchunks[cpos1].lights[(i - 255)];
                            corner_block[7] = chunks[cpos1].chunk[(i - 255)];
                        }
                    }
                    if (OPos[0] >= 1)
                    {
                        side_light[6] = lightchunks[cpos0].lights[(i + 15)];
                        side_block[6] = chunks[cpos0].chunk[(i + 15)];
                    }
                    else
                    {
                        side_light[6] = lightchunks[cpos3].lights[(i + 31)];
                        side_block[6] = chunks[cpos3].chunk[(i + 31)];
                    }
                    if (OPos[0] <= 14)
                    {
                        side_light[7] = lightchunks[cpos0].lights[(i + 17)];
                        side_block[7] = chunks[cpos0].chunk[(i + 17)];
                    }
                    else
                    {
                        side_light[7] = lightchunks[cpos1].lights[(i + 1)];
                        side_block[7] = chunks[cpos1].chunk[(i + 1)];
                    }
                }
                else
                {
                    center_light[1] = lightchunks[cpos2].lights[(i - 240)];
                    if (OPos[1] <= 255)
                    {
                        side_light[4] = lightchunks[cpos2].lights[(i + 16)];
                        side_block[4] = chunks[cpos2].chunk[(i + 16)];
                        if (OPos[0] >= 1)
                        {
                            corner_light[4] = lightchunks[cpos2].lights[(i + 15)];
                            corner_block[4] = chunks[cpos2].chunk[(i + 15)];
                        }
                        else
                        {
                            corner_light[4] = lightchunks[cpos6].lights[(i + 31)];
                            corner_block[4] = chunks[cpos6].chunk[(i + 31)];
                        }
                        if (OPos[0] <= 14)
                        {
                            corner_light[5] = lightchunks[cpos2].lights[(i + 17)];
                            corner_block[5] = chunks[cpos2].chunk[(i + 17)];
                        }
                        else
                        {
                            corner_light[5] = lightchunks[cpos5].lights[(i + 1)];
                            corner_block[5] = chunks[cpos5].chunk[(i + 1)];
                        }
                    }
                    if (OPos[1] >= 1)
                    {
                        side_light[5] = lightchunks[cpos2].lights[(i - 496)];
                        side_block[5] = chunks[cpos2].chunk[(i - 496)];

                        if (OPos[0] >= 1)
                        {
                            corner_light[6] = lightchunks[cpos2].lights[(i - 497)];
                            corner_block[6] = chunks[cpos2].chunk[(i - 497)];
                        }
                        else
                        {
                            corner_light[6] = lightchunks[cpos6].lights[(i - 481)];
                            corner_block[6] = chunks[cpos6].chunk[(i - 481)];
                        }
                        if (OPos[0] <= 14)
                        {
                            corner_light[7] = lightchunks[cpos2].lights[(i - 495)];
                            corner_block[7] = chunks[cpos2].chunk[(i - 495)];
                        }
                        else
                        {
                            corner_light[7] = lightchunks[cpos5].lights[(i - 511)];
                            corner_block[7] = chunks[cpos5].chunk[(i - 511)];
                        }
                    }
                    if (OPos[0] >= 1)
                    {
                        side_light[6] = lightchunks[cpos2].lights[(i - 241)];
                        side_block[6] = chunks[cpos2].chunk[(i - 241)];
                    }
                    else
                    {
                        side_light[6] = lightchunks[cpos6].lights[(i - 225)];
                        side_block[6] = chunks[cpos6].chunk[(i - 225)];
                    }

                    if (OPos[0] <= 14)
                    {
                        side_light[7] = lightchunks[cpos2].lights[(i - 239)];
                        side_block[7] = chunks[cpos2].chunk[(i - 239)];
                    }
                    else
                    {
                        side_light[7] = lightchunks[cpos5].lights[(i - 255)];
                        side_block[7] = chunks[cpos5].chunk[(i - 255)];
                    }
                }
                if (OPos[0] >= 1)
                {
                    center_light[2] = lightchunks[cpos0].lights[(i - 1)];
                    if (OPos[1] <= 255)
                    {
                        side_light[8] = lightchunks[cpos0].lights[(i + 255)];
                        side_block[8] = chunks[cpos0].chunk[(i + 255)];
                    }
                    if (OPos[1] >= 1)
                    {
                        side_light[9] = lightchunks[cpos0].lights[(i - 257)];
                        side_block[9] = chunks[cpos0].chunk[(i - 257)];
                    }
                }
                else
                {
                    center_light[2] = lightchunks[cpos3].lights[(i + 15)];
                    if (OPos[1] <= 255)
                    {
                        side_light[8] = lightchunks[cpos3].lights[(i + 271)];
                        side_block[8] = chunks[cpos3].chunk[(i + 271)];
                    }
                    if (OPos[1] >= 1)
                    {
                        side_light[9] = lightchunks[cpos3].lights[(i - 241)];
                        side_block[9] = chunks[cpos3].chunk[(i - 241)];
                    }
                }
                if (OPos[0] <= 14)
                {
                    center_light[3] = lightchunks[cpos0].lights[(i + 1)];
                    if (OPos[1] <= 255)
                    {
                        side_light[10] = lightchunks[cpos0].lights[(i + 257)];
                        side_block[10] = chunks[cpos0].chunk[(i + 257)];
                    }
                    if (OPos[1] >= 1)
                    {
                        side_light[11] = lightchunks[cpos0].lights[(i - 255)];
                        side_block[11] = chunks[cpos0].chunk[(i - 255)];
                    }
                }
                else
                {
                    center_light[3] = lightchunks[cpos1].lights[(i - 15)];
                    if (OPos[1] <= 255)
                    {
                        side_light[10] = lightchunks[cpos1].lights[(i + 241)];
                        side_block[10] = chunks[cpos1].chunk[(i + 241)];
                    }
                    if (OPos[1] >= 1)
                    {
                        side_light[11] = lightchunks[cpos1].lights[(i - 271)];
                        side_block[11] = chunks[cpos1].chunk[(i - 271)];
                    }
                }
                if (OPos[1] >= 1)
                {
                    center_light[4] = lightchunks[cpos0].lights[(i - 256)];
                }
                if (OPos[1] <= 255)
                {
                    center_light[5] = lightchunks[cpos0].lights[(i + 256)];
                }
                int remake_side_light[24] = {side_light[3], side_light[2], side_light[1], side_light[0], // 按照贴图坐标中的右、左、下、上的边的顺序排列
                                             side_light[6], side_light[7], side_light[5], side_light[4],
                                             side_light[2], side_light[6], side_light[9], side_light[8],
                                             side_light[7], side_light[3], side_light[11], side_light[10],
                                             side_light[9], side_light[11], side_light[5], side_light[1],
                                             side_light[8], side_light[10], side_light[4], side_light[0]};

                int remake_side_block[24] = {side_block[3], side_block[2], side_block[1], side_block[0], // 按照贴图坐标中的右、左、下、上的边的顺序排列
                                             side_block[6], side_block[7], side_block[5], side_block[4],
                                             side_block[2], side_block[6], side_block[9], side_block[8],
                                             side_block[7], side_block[3], side_block[11], side_block[10],
                                             side_block[9], side_block[11], side_block[5], side_block[1],
                                             side_block[8], side_block[10], side_block[4], side_block[0]};

                int remake_corner_light[24] = {corner_light[3], corner_light[2], corner_light[1], corner_light[0], // 按照贴图坐标中的右下、左下、右上、左上的边的顺序排列
                                               corner_light[6], corner_light[7], corner_light[4], corner_light[5],
                                               corner_light[2], corner_light[6], corner_light[0], corner_light[4],
                                               corner_light[7], corner_light[3], corner_light[5], corner_light[1],
                                               corner_light[6], corner_light[7], corner_light[2], corner_light[3],
                                               corner_light[4], corner_light[5], corner_light[0], corner_light[1]};

                int remake_corner_block[24] = {corner_block[3], corner_block[2], corner_block[1], corner_block[0], // 按照贴图坐标中的右下、左下、右上、左上的边的顺序排列
                                               corner_block[6], corner_block[7], corner_block[4], corner_block[5],
                                               corner_block[2], corner_block[6], corner_block[0], corner_block[4],
                                               corner_block[7], corner_block[3], corner_block[5], corner_block[1],
                                               corner_block[6], corner_block[7], corner_block[2], corner_block[3],
                                               corner_block[4], corner_block[5], corner_block[0], corner_block[1]};

                int anglight[24] = {};
                int aolight[24] = {};
                for (int li = 0; li < 6; li++)
                {
                    int ilight = center_light[li];
                    if (remake_corner_block[li * 4] == 0 && (remake_side_block[li * 4] == 0 || remake_side_block[li * 4 + 2] == 0)) // 检查斜角是否为空气
                    {
                        anglight[li * 4] = (ilight + remake_corner_light[li * 4]) / 2; // 该面的这个角的亮度为该面的表面亮度与斜角的亮度的平均值
                    }
                    else if (remake_side_block[li * 4] == 0 && remake_side_block[li * 4 + 2] == 0) // 检查边是否都为空气
                    {
                        anglight[li * 4] = (remake_side_light[li * 4] + remake_side_light[li * 4 + 2]) / 2; // 该面的这个角的两边的位置亮度的平均值
                    }
                    else
                    {
                        anglight[li * 4] = ((remake_side_block[li * 4] == 0 ? remake_side_light[li * 4] : ilight) + (remake_side_block[li * 4 + 2] == 0 ? remake_side_light[li * 4 + 2] : ilight)) / 2; // 哪边有方块那杯就不使用平滑
                    }
                    aolight[li * 4] = std::min(((remake_side_block[li * 4] == 0 || remake_side_block[li * 4] == 4) ? 0 : 1) + ((remake_side_block[li * 4 + 2] == 0 || remake_side_block[li * 4 + 2] == 4) ? 0 : 1) + ((remake_corner_block[li * 4] == 0 || remake_corner_block[li * 4] == 4) ? 0 : 1), 2); // 面的亮度遮挡（相当于方块间角落的阴影）

                    if (remake_corner_block[li * 4 + 1] == 0 && (remake_side_block[li * 4 + 1] == 0 || remake_side_block[li * 4 + 2] == 0))
                    {
                        anglight[li * 4 + 1] = (ilight + remake_corner_light[li * 4 + 1]) / 2;
                    }
                    else if (remake_side_block[li * 4 + 1] == 0 && remake_side_block[li * 4 + 2] == 0)
                    {
                        anglight[li * 4 + 1] = (remake_side_light[li * 4 + 1] + remake_side_light[li * 4 + 2]) / 2;
                    }
                    else
                    {
                        anglight[li * 4 + 1] = ((remake_side_block[li * 4 + 1] == 0 ? remake_side_light[li * 4 + 1] : ilight) + (remake_side_block[li * 4 + 2] == 0 ? remake_side_light[li * 4 + 2] : ilight)) / 2;
                    }
                    aolight[li * 4 + 1] = std::min(((remake_side_block[li * 4 + 1] == 0 || remake_side_block[li * 4 + 1] == 4) ? 0 : 1) + ((remake_side_block[li * 4 + 2] == 0 || remake_side_block[li * 4 + 2] == 4) ? 0 : 1) + ((remake_corner_block[li * 4 + 1] == 0 || remake_corner_block[li * 4 + 1] == 4) ? 0 : 1), 2);

                    if (remake_corner_block[li * 4 + 2] == 0 && (remake_side_block[li * 4] == 0 || remake_side_block[li * 4 + 3] == 0))
                    {
                        anglight[li * 4 + 2] = (ilight + remake_corner_light[li * 4 + 2]) / 2;
                    }
                    else if (remake_side_block[li * 4] == 0 && remake_side_block[li * 4 + 3] == 0)
                    {
                        anglight[li * 4 + 2] = (remake_side_light[li * 4] + remake_side_light[li * 4 + 3]) / 2;
                    }
                    else
                    {
                        anglight[li * 4 + 2] = ((remake_side_block[li * 4] == 0 ? remake_side_light[li * 4] : ilight) + (remake_side_block[li * 4 + 3] == 0 ? remake_side_light[li * 4 + 3] : ilight)) / 2;
                    }
                    aolight[li * 4 + 2] = std::min(((remake_side_block[li * 4] == 0 || remake_side_block[li * 4] == 4) ? 0 : 1) + ((remake_side_block[li * 4 + 3] == 0 || remake_side_block[li * 4 + 3] == 4) ? 0 : 1) + ((remake_corner_block[li * 4 + 2] == 0 || remake_corner_block[li * 4 + 2] == 4) ? 0 : 1), 2);

                    if (remake_corner_block[li * 4 + 3] == 0 && (remake_side_block[li * 4 + 1] == 0 || remake_side_block[li * 4 + 3] == 0))
                    {
                        anglight[li * 4 + 3] = (ilight + remake_corner_light[li * 4 + 3]) / 2;
                    }
                    else if (remake_side_block[li * 4 + 1] == 0 && remake_side_block[li * 4 + 3] == 0)
                    {
                        anglight[li * 4 + 3] = (remake_side_light[li * 4 + 1] + remake_side_light[li * 4 + 3]) / 2;
                    }
                    else
                    {
                        anglight[li * 4 + 3] = ((remake_side_block[li * 4 + 1] == 0 ? remake_side_light[li * 4 + 1] : ilight) + (remake_side_block[li * 4 + 3] == 0 ? remake_side_light[li * 4 + 3] : ilight)) / 2;
                    }
                    aolight[li * 4 + 3] = std::min(((remake_side_block[li * 4 + 1] == 0 || remake_side_block[li * 4 + 1] == 4) ? 0 : 1) + ((remake_side_block[li * 4 + 3] == 0 || remake_side_block[li * 4 + 3] == 4) ? 0 : 1) + ((remake_corner_block[li * 4 + 3] == 0 || remake_corner_block[li * 4 + 3] == 4) ? 0 : 1), 2);
                }
                chunks[cpos0].display_chunk.push_back(i);                      // 将方块的区块坐标放入显示数据中
                chunks[cpos0].display_chunk.push_back(chunks[cpos0].chunk[i]); // 将方块的ID放入显示数据中
                chunks[cpos0].display_chunk.push_back(g);                      // 将方块要显示哪些面的信息放入显示数据中

                temp = 0;
                for (int li = 0; li < 6; li++)
                {
                    temp += center_light[li] * pow16[li]; // 将方块六个表面亮度存入一个数字
                }
                chunks[cpos0].display_chunk.push_back(temp); // 将方块表面亮度放入显示数据中

                for (int li = 0; li < 6; li++)
                {
                    temp = 0;
                    for (int lj = 0; lj < 4; lj++)
                    {
                        temp += anglight[li * 4 + lj] * pow16[lj]; // 将方块四个角的亮度存入一个数字
                    }
                    chunks[cpos0].display_chunk.push_back(temp); // 将方块表面的角的亮度放入显示数据中
                }
                for (int li = 0; li < 6; li++)
                {
                    temp = 0;
                    for (int lj = 0; lj < 4; lj++)
                    {
                        temp += aolight[li * 4 + lj] * pow16[lj]; // 将方块四个角的面遮挡存入一个数字
                    }
                    chunks[cpos0].display_chunk.push_back(temp); // 将方块表面的角的面遮挡放入显示数据中
                }
            }
        }
    }
}

/*void Chunks::light_check(int cpos)
{
    int cx, cy;
    cx = cpos % 21 + 1 + offset[0];
    cy = cpos / 21 + 1 + offset[1];
    std::string str0 = std::to_string(cx) + " " + std::to_string(cy);


    for (int n = 15; n > 0; n--)
    {
        for (auto l : lights[str0][n])
        {
            flood_fill_light(cx * 16 + l % 16, l / 256, cy*16 + l % 256 / 16,n);
        }
    }
}*/

void Chunks::light_check(int cpos)
{
    int cx, cy;
    cx = cpos % 23 + offset[0];
    cy = cpos / 23 + offset[1];
    ChunkPos chunk_pos = {cx, cy};
    std::vector<ChunkOP> light_list, temp_light_list;
    for (int n = 14; n > 0; n--)
    {

        temp_light_list.clear();
        for (auto l : lights[chunk_pos][n + 1])
        {
            light_list.push_back({chunk_pos, l});
        }
        for (auto it : light_list)
        {
            int i = it.index;
            ChunkPos itstr = it.pos;

            int OPos[3] = {i % 16, i / 256, i % 256 / 16};
            if (OPos[2] >= 1) // 向前延伸
            {
                if ((chunks[itstr].chunk[(i - 16)] == 0) && lightchunks[itstr].lights[(i - 16)] < n)
                {
                    lightchunks[itstr].lights[(i - 16)] = n;
                    temp_light_list.push_back({itstr, i - 16});
                }
            }
            else
            {
                ChunkPos ocpos = offset_cPos(itstr, 0, -1);
                if ((chunks[ocpos].chunk[(i + 240)] == 0) && lightchunks[ocpos].lights[(i + 240)] < n)
                {
                    lightchunks[ocpos].lights[(i + 240)] = n;
                    temp_light_list.push_back({ocpos, i + 240});
                }
            }

            if (OPos[2] <= 14) // 向后延伸
            {
                if ((chunks[itstr].chunk[(i + 16)] == 0) && lightchunks[itstr].lights[(i + 16)] < n)
                {
                    lightchunks[itstr].lights[(i + 16)] = n;
                    temp_light_list.push_back({itstr, i + 16});
                }
            }
            else
            {
                ChunkPos ocpos = offset_cPos(itstr, 0, 1);
                if ((chunks[ocpos].chunk[(i - 240)] == 0) && lightchunks[ocpos].lights[(i - 240)] < n)
                {
                    lightchunks[ocpos].lights[(i - 240)] = n;
                    temp_light_list.push_back({ocpos, i - 240});
                }
            }
            if (OPos[0] >= 1) // 向左延伸
            {
                if ((chunks[itstr].chunk[(i - 1)] == 0) && lightchunks[itstr].lights[(i - 1)] < n)
                {
                    lightchunks[itstr].lights[(i - 1)] = n;
                    temp_light_list.push_back({itstr, i - 1});
                }
            }
            else
            {
                ChunkPos ocpos = offset_cPos(itstr, -1, 0);
                if ((chunks[ocpos].chunk[(i + 15)] == 0) && lightchunks[ocpos].lights[(i + 15)] < n)
                {
                    lightchunks[ocpos].lights[(i + 15)] = n;
                    temp_light_list.push_back({ocpos, i + 15});
                }
            }
            if (OPos[0] <= 14) // 向右延伸
            {
                if ((chunks[itstr].chunk[(i + 1)] == 0) && lightchunks[itstr].lights[(i + 1)] < n)
                {
                    lightchunks[itstr].lights[(i + 1)] = n;
                    temp_light_list.push_back({itstr, i + 1});
                }
            }
            else
            {
                ChunkPos ocpos = offset_cPos(itstr, 1, 0);
                if ((chunks[ocpos].chunk[(i - 15)] == 0) && lightchunks[ocpos].lights[(i - 15)] < n)
                {
                    lightchunks[ocpos].lights[(i - 15)] = n;
                    temp_light_list.push_back({ocpos, i - 15});
                }
            }

            if (OPos[1] >= 1) // 向下延伸
            {
                if ((chunks[itstr].chunk[(i - 256)] == 0) && lightchunks[itstr].lights[(i - 256)] < n)
                {
                    lightchunks[itstr].lights[(i - 256)] = n;
                    temp_light_list.push_back({itstr, i - 256});
                }
            }

            if (OPos[1] <= 255) // 向上延伸
            {
                if ((chunks[itstr].chunk[(i + 256)] == 0) && lightchunks[itstr].lights[(i + 256)] < n)
                {
                    lightchunks[itstr].lights[(i + 256)] = n;
                    temp_light_list.push_back({itstr, i + 256});
                }
            }
        }
        light_list = temp_light_list;
    }
}

void Chunks::load()
{
    cltm();

    std::thread thread_read(&Chunks::reading, this);
    thread_read.detach();
    std::thread thread_recheck(&Chunks::rechecking, this);
    thread_recheck.detach();
}

void Chunks::rechecking()
{
    while (gamerun)
    {
        int offsetore[2];
        offsetore[0] = toffset[0];
        offsetore[1] = toffset[1];
        for (int jt = 0; jt < 529; jt++)
        {
            int j = linetmap[jt];
            ChunkPos cpos = {j % 23 + offsetore[0], j / 23 + offsetore[1]};
            mapMutex.lock();
            if (chunks.find(cpos) != chunks.end())
            {
                if (chunks[cpos].read && (!chunks[cpos].lcheck))
                {
                    light_check(j);
                    chunks[cpos].lcheck = true;
                    jt = 529;
                }
                mapMutex.unlock();
            }
            else
            {
                mapMutex.unlock();
            }
        }
    }
    recheckthreadover = true;
}

void Chunks::reading()
{
    while (gamerun)
    {

        offset[0] = toffset[0];
        offset[1] = toffset[1];
        for (auto iter = begin(chunks); iter != end(chunks);)
        {
            int p[2];
            p[0] = iter->first.x;
            p[1] = iter->first.y;

            if (offset[0] > p[0] || p[0] > offset[0] + 23 || offset[1] > p[1] || p[1] > offset[1] + 23)
            {
                mapMutex.lock();
                iter = chunks.erase(iter);
                mapMutex.unlock();
            }
            else
            {
                ++iter;
            }
        }
        for (auto iter = begin(lightchunks); iter != end(lightchunks);)
        {
            int p[2];
            p[0] = iter->first.x;
            p[1] = iter->first.y;

            if (offset[0] > p[0] || p[0] > offset[0] + 23 || offset[1] > p[1] || p[1] > offset[1] + 23)
            {
                mapMutex.lock();
                iter = lightchunks.erase(iter);
                mapMutex.unlock();
            }
            else
            {
                ++iter;
            }
        }
        for (auto iter = begin(lights); iter != end(lights);)
        {
            int p[2];
            p[0] = iter->first.x;
            p[1] = iter->first.y;

            if (offset[0] > p[0] || p[0] > offset[0] + 23 || offset[1] > p[1] || p[1] > offset[1] + 23)
            {
                mapMutex.lock();
                iter = lights.erase(iter);
                mapMutex.unlock();
            }
            else
            {
                ++iter;
            }
        }

        for (int k = 0; k < 529; k++)
        {
            int i = linetmap[k];
            ChunkPos str = {i % 23 + offset[0], i / 23 + offset[1]};
            mapMutex.lock();
            if (chunks.find(str) == chunks.end())
            {
                if (!chunks[str].read)
                {
                    read(i);
                }
                mapMutex.unlock();
                for (int jt = 0; jt < 529; jt++)
                {
                    int j = linetmap[jt];
                    int cx = j % 21 + 1 + offset[0];
                    int cy = j / 21 + 1 + offset[1];
                    ChunkPos cpos0 = {cx, cy};
                    ChunkPos cpos1 = {cx + 1, cy};
                    ChunkPos cpos2 = {cx, cy + 1};
                    ChunkPos cpos3 = {cx - 1, cy};
                    ChunkPos cpos4 = {cx, cy - 1};
                    ChunkPos cpos5 = {cx + 1, cy + 1};
                    ChunkPos cpos6 = {cx - 1, cy + 1};
                    ChunkPos cpos7 = {cx - 1, cy - 1};
                    ChunkPos cpos8 = {cx + 1, cy - 1};

                    mapMutex.lock();
                    if (chunks.find(cpos0) != chunks.end() && chunks.find(cpos1) != chunks.end() && chunks.find(cpos2) != chunks.end() && chunks.find(cpos3) != chunks.end() && chunks.find(cpos4) != chunks.end() && chunks.find(cpos5) != chunks.end() && chunks.find(cpos6) != chunks.end() && chunks.find(cpos7) != chunks.end() && chunks.find(cpos8) != chunks.end())
                    {
                        if (chunks[cpos0].lcheck && chunks[cpos1].lcheck && chunks[cpos2].lcheck && chunks[cpos3].lcheck && chunks[cpos4].lcheck && chunks[cpos5].lcheck && chunks[cpos6].lcheck && chunks[cpos7].lcheck && chunks[cpos8].lcheck)
                        {
                            if (!chunks[cpos0].check)
                            {
                                check(j);
                                chunks[cpos0].check = true;
                                jt = 529;
                            }
                        }
                        if (!chunks[cpos0].read)
                        {
                            chunks.erase(cpos0);
                        }
                    }
                    mapMutex.unlock();
                }
                k = 529;
            }
            else
            {
                mapMutex.unlock();
            }
        }
    }
    readthreadover = true;
}

double *Chunks::move_array(double p[2], double a, double b)
{
    double *r = new double[2];
    r[0] = p[0] + a;
    r[1] = p[1] + b;
    double d[2] = {p[0] + a, p[1] + b};
    return r;
}
double Chunks::dot(double a[2], double b[2])
{
    double c[4] = {a[0], b[0], a[1], b[1]};

    return a[0] * b[0] + a[1] * b[1];
}

double Chunks::fade(double x)
{
    return x * x * x * (x * (6 * x - 15) + 10);
}

double Chunks::mix(double x, double y, double z)
{
    return x + (y - x) * z;
}
double Chunks::perlin(double pos[2], double k[2], int key)
{
    double posk[2], kk[2], u1[2], u[2], a, b, c, d, base;
    int n;

    posk[0] = floor(pos[0] / 16);
    posk[1] = floor(pos[1] / 16);
    kk[0] = (fmod(fmod(pos[0], 16.0) + 16, 16.0) + k[0]) / 16.0;
    kk[1] = (fmod(fmod(pos[1], 16.0) + 16, 16.0) + k[1]) / 16.0;
    u1[0] = fade(kk[0]);
    u1[1] = fade(kk[1]);
    a = dot(random(move_array(posk, 0.0, 0.0), key), move_array(kk, 0.0, 0.0));
    b = dot(random(move_array(posk, 1.0, 0.0), key), move_array(kk, -1.0, 0.0));
    c = dot(random(move_array(posk, 0.0, 1.0), key), move_array(kk, 0.0, -1.0));
    d = dot(random(move_array(posk, 1.0, 1.0), key), move_array(kk, -1.0, -1.0));
    base = std::max(mix(mix(a, b, u1[0]), mix(c, d, u1[0]), u1[1]) + 1, 0.0) * 50;
    key += 30;

    posk[0] = floor(pos[0] / 4);
    posk[1] = floor(pos[1] / 4);
    kk[0] = (fmod(fmod(pos[0], 4.0) + 4, 4.0) + k[0]) / 4.0;
    kk[1] = (fmod(fmod(pos[1], 4.0) + 4, 4.0) + k[1]) / 4.0;
    u1[0] = fade(kk[0]);
    u1[1] = fade(kk[1]);
    a = dot(random(move_array(posk, 0.0, 0.0), key), move_array(kk, 0.0, 0.0));
    b = dot(random(move_array(posk, 1.0, 0.0), key), move_array(kk, -1.0, 0.0));
    c = dot(random(move_array(posk, 0.0, 1.0), key), move_array(kk, 0.0, -1.0));
    d = dot(random(move_array(posk, 1.0, 1.0), key), move_array(kk, -1.0, -1.0));

    base += std::max(mix(mix(a, b, u1[0]), mix(c, d, u1[0]), u1[1]), -1.0) * n;

    key += 30;
    a = dot(random(move_array(pos, 0.0, 0.0), key), move_array(k, 0.0, 0.0));
    b = dot(random(move_array(pos, 1.0, 0.0), key), move_array(k, -1.0, 0.0));
    c = dot(random(move_array(pos, 0.0, 1.0), key), move_array(k, 0.0, -1.0));
    d = dot(random(move_array(pos, 1.0, 1.0), key), move_array(k, -1.0, -1.0));
    u[0] = fade(k[0]);
    u[1] = fade(k[1]);
    if (base > 28)
        n = 3;
    else if (base > 0)
        n = 4;
    else if (base > -8)
        n = 2;
    else if (base > -18)
        n = 1;
    else if (base >= -30)
        n = 2;
    base += std::max(mix(mix(a, b, u[0]), mix(c, d, u[0]), u[1]), -1.0) * n;

    return base;
}

double *Chunks::random(double k[2], int nkey)
{
    double k1[] = {nkey / 47699450 * 1.0, nkey % 1045842 * 1.0};
    double k2[] = {nkey % 9245745 * 1.0, nkey % 75929544 * 1.0};
    double dk[] = {k[0] * 1.0, k[1] * 1.0};
    double a[] = {dot(dk, k1) + nkey % 1045842, dot(dk, k2) + nkey % 75929544};
    double *r = new double[2];
    r[0] = fmod(nkey % 2858397 * sin(a[0]), 1.0) * 2 - 1;
    r[1] = fmod(nkey % 1734983 * sin(a[1]), 1.0) * 2 - 1;

    return r;
}

void Chunks::flood_fill_light(int x, int y, int z, int color)
{
    if (color > 1)
    {
        ChunkOP own = Get_BlockCO(x, y, z);
        lightchunks[own.pos].lights[own.index] = color;

        color -= 1;
        ChunkOP up = Get_BlockCO(x, y + 1, z);
        ChunkOP down = Get_BlockCO(x, y - 1, z);
        ChunkOP left = Get_BlockCO(x - 1, y, z);
        ChunkOP right = Get_BlockCO(x + 1, y, z);
        ChunkOP front = Get_BlockCO(x, y, z - 1);
        ChunkOP back = Get_BlockCO(x, y, z + 1);
        if (y < 256 && lightchunks[up.pos].lights[up.index] < color)
            flood_fill_light(x, y + 1, z, color);
        if (y > 1 && lightchunks[down.pos].lights[down.index] < color)
            flood_fill_light(x, y - 1, z, color);
        if (lightchunks[left.pos].lights[left.index] < color)
            flood_fill_light(x - 1, y, z, color);
        if (lightchunks[right.pos].lights[right.index] < color)
            flood_fill_light(x + 1, y, z, color);
        if (lightchunks[front.pos].lights[front.index] < color)
            flood_fill_light(x, y, z - 1, color);
        if (lightchunks[back.pos].lights[back.index] < color)
            flood_fill_light(x, y, z + 1, color);
    }
}