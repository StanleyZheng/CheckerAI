//
// CheckersAI.cpp
// Stanley Zheng
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <unistd.h>
using namespace std;

#define UINT unsigned int
#define INFTY_P numeric_limits<int>::max()
#define INFTY_N numeric_limits<int>::lowest()
#define WHITE 0
#define BLACK 1
#define HUMAN 0
#define COMPUTER 1

double cpu_time;
int cpu_timelimit;
int cpu_maxdepth;

int root_depth;
bool is_leaf_node;
volatile sig_atomic_t cpu_time_up;
void signalHandler(int signum) {
    cpu_time_up = true;
    return;
}

// Single bit mask array / Piece to bitboard converter
UINT S[32];

// Lookup Tables for getting the squares relative to piece at index number
UINT Up_Left[32] =  {
                   99,   99,   99,   99,
                99,    0,    1,    2,
                    4,    5,    6,    7,
                99,    8,    9,   10,   
                   12,   13,   14,   15,
                99,   16,   17,   18,   
                   20,   21,   22,   23,
                99,   24,   25,   26
                };
UINT Up_Right[32] =  {
                   99,   99,   99,   99,
                 0,    1,    2,    3,
                    5,    6,    7,   99,
                 8,    9,   10,   11,
                   13,   14,   15,   99,
                16,   17,   18,   19,
                   21,   22,   23,   99,
                24,   25,   26,   27
                };
UINT Down_Left[32] = {
                    4,    5,    6,    7,
                99,    8,    9,   10,   
                   12,   13,   14,   15,
                99,   16,   17,   18,   
                   20,   21,   22,   23,
                99,   24,   25,   26,   
                   28,   29,   30,   31,
                99,   99,   99,   99   
                };
UINT Down_Right[32] = {   
                    5,    6,    7,   99,
                 8,    9,   10,   11,
                   13,   14,   15,   99,
                16,   17,   18,   19,
                   21,   22,   23,   99,
                24,   25,   26,   27,
                   29,   30,   31,   99,
                99,   99,   99,   99
                };


class Game {

    // Masks for moving pieces, top/bottom row for promotions, and special positions on board
    UINT MASK_L3, MASK_L4, MASK_L5, MASK_R3, MASK_R4, MASK_R5;
    UINT MASK_TOP, MASK_BOT;
    UINT MASK_EDGES;
    UINT MASK_DBLCORNER1, MASK_DBLCORNER2;
    
    // Look-up tables for 16 bit numbers
    unsigned char bitCount_Tbl[65536];
    unsigned char msb_Tbl[65536];
    unsigned char lsb_Tbl[65536];

    // Move class for holding information about a single move
    struct Move {

        // Start and end square numbers
        UINT start, end;

        // Bitboards of the locations that change
        // XOR these with the current board to get next board
        UINT WM, BM, KM;

        Move() {}

        Move(UINT start, UINT end) {
            this->start = start;
            this->end = end;
        }

        Move(UINT start, UINT end, UINT WM, UINT BM, UINT KM) {
            this->start = start;
            this->end = end;
            this->WM = WM;
            this->BM = BM;
            this->KM = KM;
        }
        
        bool operator==(const Move &other) {
            return (start == other.start) ? (end == other.end) : false;
        }

        Move& operator=(const Move &other) {
            if (this != &other) {
                start = other.start;
                end = other.end;
                WM = other.WM;
                BM = other.BM;
                KM = other.KM;
            }
            return *this;
        }
    };


    //
    // GAME INFO
    //
    int White_Player;
    int BlacK_Player;
    UINT m_turn;
    int m_turn_num;

    // Bitboards for White, Black, and Kings
    UINT m_WP, m_BP, m_K;

    // Variable used for updating end position for multiple jumps in single turn
    UINT end_temp;

    Move best_move, best_move_temp;
    vector<Move> m_moves;

public:
    Game() {

        // Numbers representing the bit positions
        /*
        Black on top  
          00  01  02  03 
        04  05  06  07
          08  09  10  11
        12  13  14  15
          16  17  18  19
        20  21  22  23  
          24  25  26  27 
        28  29  30  31  
        White on bottom 
        */

        // Bit masks from 1st to 31st bit
        S[0] = 1;
        for(UINT i = 1; i < 32; i++)
            S[i] = S[i-1] * 2;

        // Mask for pieces MOVING DOWN
        MASK_L3 = S[ 5] | S[ 6] | S[ 7] | S[13] | S[14] | S[15] | S[21] | S[22] | S[23];
        MASK_L5 = S[ 0] | S[ 1] | S[ 2] | S[ 8] | S[ 9] | S[10] | S[16] | S[17] | S[18] | S[24] | S[25] | S[26];

        // Mask for pieces MOVING UP
        MASK_R3 = S[ 8] | S[ 9] | S[10] | S[16] | S[17] | S[18] | S[24] | S[25] | S[26];
        MASK_R5 = S[ 5] | S[ 6] | S[ 7] | S[13] | S[14] | S[15] | S[21] | S[22] | S[23] | S[29] | S[30] | S[31];

        // All other possibilties are covered by LS4 (for pieces MOVING DOWN) or RS4 (for pieces MOVING UP)

        // Masks for checking if top or bottom row
        MASK_TOP = S[ 0] | S[ 1] | S[ 2] | S[ 3];
        MASK_BOT = S[28] | S[29] | S[30] | S[31];

        // Masks for edges of board
        MASK_EDGES =   S[ 0] | S[ 1] | S[ 2] | S[ 3]
                     | S[ 4]                 | S[11]
                     | S[12]                 | S[19]
                     | S[20]                 | S[27]
                     | S[28] | S[29] | S[30] | S[31];

        // Masks for corners
        MASK_DBLCORNER1 = S[ 0] | S[ 4];
        MASK_DBLCORNER2 = S[27] | S[31];

        // Initialize look-up tables
        for(int bb_half = 0; bb_half < 65536; bb_half++) {
            int bitCount = 0, msb = 255, lsb = 255;
            for (int i = 0; i < 16; i++) {
                if (bb_half & S[i]) {
                    bitCount++;
                    if (lsb == 255) {
                        lsb = i;
                    }
                    msb = i;
                }
            }
            bitCount_Tbl[bb_half] = bitCount;
            msb_Tbl[bb_half] = msb;
            lsb_Tbl[bb_half] = lsb;
        }
    }


    //
    // LOADING BOARDS
    //
    // Initialize board
    void init_board(UINT &WP, UINT &BP, UINT &K) {
        WP = 0xFFF00000;
        BP = 0x00000FFF;
        K = 0;
    }
    // Load custom board
    void cust_board(UINT &WP, UINT &BP, UINT &K, UINT &turn, int &time) {
        ifstream board_file;
        string file_name;

        while(true) {
            cout << "Enter file name: ";
            if(!(cin >> file_name)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            }
            else {
                board_file.open(file_name);
                if(!board_file)
                    cerr << "Error: Cannot open file." << endl;
                else {
                    cout << endl;
                    break;
                }
            }
        }

        int row = 0, i = 0, piece;
        WP = BP = K = 0;
        while(board_file >> piece) {
            if(piece == 0) {
            }
            else if(piece == 1)
                WP |= (1 << i);
            else if(piece == 2)
                BP |= (1 << i);
            else if(piece == 3) {
                WP |= (1 << i);
                K |= (1 << i);
            }
            else {
                BP |= (1 << i);
                K |= (1 << i);
            }

            i++;
            if(i % 4 == 0)
                row++;

            if(row == 8)
                break;
        }

        board_file >> turn;
        turn--;
        board_file >> time;
    }


    //
    // HELPER FUNCTIONS FOR BIT OPERATIONS
    //
    UINT get_bit_count(UINT i) {
        if (i == 0)
            return 0;
        return bitCount_Tbl[i & 65535] + bitCount_Tbl[(i >> 16) & 65535];
    }
    UINT get_lsb(UINT i) {
        if(i & 65535)
            return lsb_Tbl[i & 65535];
        if((i >> 16) & 65535)
            return lsb_Tbl[(i >> 16) & 65535] + 16;
        return 0;
    }
    UINT get_msb(UINT i) {
        if((i >> 16) & 65535)
            return msb_Tbl[(i >> 16) & 65535] + 16;
        if(i & 65535)
            return msb_Tbl[i & 65535];
        return 0;
    }


    //
    // GET PIECES THAT CAN MOVE, WALK/JUMP
    //
    UINT get_walkers_W(UINT WP, UINT BP, UINT K) {
        const UINT UOCC = ~(WP|BP);
        const UINT WK = WP&K;
        UINT walkers = ((UOCC << 4) & WP) | (((UOCC & MASK_L3) << 3) & WP) | (((UOCC & MASK_L5) << 5) & WP);
        if(WK) 
            walkers |= ((UOCC >> 4) & WK) | (((UOCC & MASK_R3) >> 3) & WK) | (((UOCC & MASK_R5) >> 5) & WK);
        return walkers;
    }
    UINT get_walkers_B(UINT WP, UINT BP, UINT K) {
        const UINT UOCC = ~(WP|BP);
        const UINT BK = BP&K;
        UINT walkers = ((UOCC >> 4) & BP) | (((UOCC & MASK_R3) >> 3) & BP) | (((UOCC & MASK_R5) >> 5) & BP);
        if(BK)
            walkers |= ((UOCC << 4) & BK) | (((UOCC & MASK_L3) << 3) & BK) | (((UOCC & MASK_L5) << 5) & BK);
        return walkers;
    }
    UINT get_jumpers_W(UINT WP, UINT BP, UINT K) {
        const UINT UOCC = ~(WP|BP);
        const UINT WK = WP&K;
        UINT jumpers = 0;

        // Check empty against opponent piece, then check against own piece
        UINT temp = (UOCC << 4) & BP;
        if(temp)
            jumpers |= (((temp & MASK_L3) << 3) | ((temp & MASK_L5) << 5)) & WP;
        
        temp = (((UOCC & MASK_L3) << 3) | ((UOCC & MASK_L5) << 5)) & BP;
        if(temp)
            jumpers |= (temp << 4) & WP;

        if(WK) {
            temp = (UOCC >> 4) & BP;
            if(temp)
                jumpers |= (((temp & MASK_R3) >> 3) | ((temp & MASK_R5) >> 5)) & WK;

            temp = (((UOCC & MASK_R3) >> 3) | ((UOCC & MASK_R5) >> 5)) & BP;
            if(temp)
                jumpers |= (temp >> 4) & WK;
        }

        return jumpers;
    }
    UINT get_jumpers_B(UINT WP, UINT BP, UINT K) {
        const UINT UOCC = ~(WP|BP);
        const UINT BK = BP&K;
        UINT jumpers = 0;

        UINT temp = (UOCC >> 4) & WP;
        if(temp)
            jumpers |= (((temp & MASK_R3) >> 3) | ((temp & MASK_R5) >> 5)) & BP;
        
        temp = (((UOCC & MASK_R3) >> 3) | ((UOCC & MASK_R5) >> 5)) & WP;
        if(temp)
            jumpers |= (temp >> 4) & BP;
        
        if(BK) {
            temp = (UOCC << 4) & WP;
            if(temp)
                jumpers |= (((temp & MASK_L3) << 3) | ((temp & MASK_L5) << 5)) & BK;

            temp = (((UOCC & MASK_L3) << 3) | ((UOCC & MASK_L5) << 5)) & WP;
            if(temp)
                jumpers |= (temp << 4) & BK;
        }

        return jumpers;
    }

    //
    // GET ALL LEGAL MOVES, WALKS/JUMPS
    //
    void get_walks_W(UINT walker_num, UINT WP, UINT BP, UINT K, vector<Move> &moves) {
        UINT end;
        UINT WM, KM;
        UINT walker_bb = S[walker_num];
        UINT UOCC = ~(WP|BP);
        UINT is_king = walker_bb & K;
        UINT next_sq, next_sq_bb;
        UINT is_empty, is_promote;

        // Check if pieces can move Up-Left
        next_sq = Up_Left[walker_num];
        if(next_sq != 99) {
            next_sq_bb = S[next_sq];
            is_empty = next_sq_bb & UOCC;
            if(is_empty) {
                WM = walker_bb | next_sq_bb;
                if(is_king)
                    KM = WM;
                else {
                    is_promote = next_sq_bb & MASK_TOP;
                    if(is_promote)
                        KM = next_sq_bb;
                    else
                        KM = 0;
                }
                moves.push_back(Move(walker_num,next_sq,WM,0,KM));
            }
        }

        // Check if pieces can move Up-Right
        next_sq = Up_Right[walker_num];
        if(next_sq != 99) {
            next_sq_bb = S[next_sq];
            is_empty = next_sq_bb & UOCC;
            if(is_empty) {
                WM = walker_bb | next_sq_bb;
                if(is_king)
                    KM = WM;
                else {
                    is_promote = next_sq_bb & MASK_TOP;
                    if(is_promote)
                        KM = next_sq_bb;
                    else
                        KM = 0;
                }
                moves.push_back(Move(walker_num,next_sq,WM,0,KM));
            }
        }

        // Check if pieces are Kings and then check Down-Left and Down-Right
        if(is_king) {
            next_sq = Down_Left[walker_num];
            if(next_sq != 99) {
                next_sq_bb = S[next_sq];
                is_empty = next_sq_bb & UOCC;
                if(is_empty) {
                    WM = walker_bb | next_sq_bb;
                    KM = WM;
                    moves.push_back(Move(walker_num,next_sq,WM,0,KM));
                }
            }

            next_sq = Down_Right[walker_num];
            if(next_sq != 99) {
                next_sq_bb = S[next_sq];
                is_empty = next_sq_bb & UOCC;
                if(is_empty) {
                    WM = walker_bb | next_sq_bb;
                    KM = WM;
                    moves.push_back(Move(walker_num,next_sq,WM,0,KM));
                }
            }
        }
    }
    void get_walks_B(UINT walker_num, UINT WP, UINT BP, UINT K, vector<Move> &moves) {
        UINT BM, KM;
        UINT walker_bb = S[walker_num];
        UINT UOCC = ~(WP|BP);
        UINT is_king = walker_bb & K;
        UINT next_sq, next_sq_bb;
        UINT is_empty, is_promote;

        // Check if pieces can move Down-Left
        next_sq = Down_Left[walker_num];
        if(next_sq != 99) {
            next_sq_bb = S[next_sq];
            is_empty = next_sq_bb & UOCC;
            if(is_empty) {
                BM = walker_bb | next_sq_bb;
                if(is_king)
                    KM = BM;
                else {
                    is_promote = next_sq_bb & MASK_BOT;
                    if(is_promote)
                        KM = next_sq_bb;
                    else 
                        KM = 0;
                }
                moves.push_back(Move(walker_num,next_sq,0,BM,KM));
            }
        }

        // Check if pieces can move Down-Right
        next_sq = Down_Right[walker_num];
        if(next_sq != 99) {
            next_sq_bb = S[next_sq];
            is_empty = next_sq_bb & UOCC;
            if(is_empty) {
                BM = walker_bb | next_sq_bb;
                if(is_king)
                    KM = BM;
                else {
                    is_promote = next_sq_bb & MASK_BOT;
                    if(is_promote)
                        KM = next_sq_bb;
                    else 
                        KM = 0;
                }
                moves.push_back(Move(walker_num,next_sq,0,BM,KM));
            }
        }

        // Check if pieces are Kings and then check Up-Left and Up-Right
        if(is_king) {
            next_sq = Up_Left[walker_num];
            if(next_sq != 99) {
                next_sq_bb = S[next_sq];
                is_empty = next_sq_bb & UOCC;
                if(is_empty) {
                    BM = walker_bb | next_sq_bb;
                    KM = BM;
                    moves.push_back(Move(walker_num,next_sq,0,BM,KM));
                }
            }

            next_sq = Up_Right[walker_num];
            if(next_sq != 99) {
                next_sq_bb = S[next_sq];
                is_empty = next_sq_bb & UOCC;
                if(is_empty) {
                    BM = walker_bb | next_sq_bb;
                    KM = BM;
                    moves.push_back(Move(walker_num,next_sq,0,BM,KM));
                }
            }
        }
    }
    void get_jumps_W(UINT jumper_num, UINT WP, UINT BP, UINT K, UINT WP_orig, UINT BP_orig, UINT K_orig, UINT start, UINT &end, vector<Move> &moves) {
        UINT WM, BM, KM;
        UINT jumper_bb = S[jumper_num];
        UINT UOCC = ~(WP|BP);
        UINT is_king = jumper_bb & K;
        UINT next_sq, next_sq_bb;
        UINT jump_sq, jump_sq_bb;
        UINT is_opp, is_empty, is_promote;
        bool same_turn = true;
        Move move;

        // Check if pieces can jump Up-Left
        next_sq = Up_Left[jumper_num];
        if(next_sq != 99) {
            next_sq_bb = S[next_sq];
            is_opp = next_sq_bb & BP;
            if(is_opp) {
                jump_sq = Up_Left[get_msb(next_sq_bb)];
                if(jump_sq != 99) {
                    jump_sq_bb = S[jump_sq];
                    is_empty = jump_sq_bb & UOCC;
                    if(is_empty) {
                        end = jump_sq;
                        WM = jumper_bb | jump_sq_bb;
                        BM = next_sq_bb;
                        KM = BM & K;
                        // Check for a promotion if on other size
                        if(!is_king) {
                            is_promote = jump_sq_bb & MASK_TOP;
                            if(is_promote) {
                                KM |= jump_sq_bb;
                                same_turn = false;
                            }
                            else {
                                get_jumps_W(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);  // K^0 = K
                                same_turn = false;
                            }
                        }
                        else {
                            KM |= WM;
                            get_jumps_W(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                            same_turn = false;
                        }
                    }
                }
            }
        }
        if(!same_turn) {
            move = Move(start,end,WP_orig^WP^WM,BP_orig^BP^BM,K_orig^K^KM);
            if(find(moves.begin(),moves.end(),move) == moves.end())
                moves.push_back(move);
        }

        // Check if pieces can jump Up-Right
        next_sq = Up_Right[jumper_num];
        if(next_sq != 99) {
            next_sq_bb = S[next_sq];
            is_opp = next_sq_bb & BP;
            if(is_opp) {
                jump_sq = Up_Right[get_msb(next_sq_bb)];
                if(jump_sq != 99) {
                    jump_sq_bb = S[jump_sq];
                    is_empty = jump_sq_bb & UOCC;
                    if(is_empty) {
                        end = jump_sq;
                        WM = jumper_bb | jump_sq_bb;
                        BM = next_sq_bb;
                        KM = BM & K;
                        if(!is_king) {
                            is_promote = jump_sq_bb & MASK_TOP;
                            if(is_promote) {
                                KM |= jump_sq_bb;
                                same_turn = false;
                            }
                            else {
                                get_jumps_W(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                                same_turn = false;
                            }
                        }
                        else {
                            KM |= WM;
                            get_jumps_W(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                            same_turn = false;
                        }
                    }
                }
            }
        }
        if(!same_turn) {
            move = Move(start,end,WP_orig^WP^WM,BP_orig^BP^BM,K_orig^K^KM);
            if(find(moves.begin(),moves.end(),move) == moves.end())
                moves.push_back(move);
        }

        // Check if pieces are Kings and then check Down-Left and Down-Right
        if(is_king) {
            next_sq = Down_Left[jumper_num];
            if(next_sq != 99) {
                next_sq_bb = S[next_sq];
                is_opp = next_sq_bb & BP;
                if(is_opp) {
                    jump_sq = Down_Left[get_msb(next_sq_bb)];
                    if(jump_sq != 99) {
                        jump_sq_bb = S[jump_sq];
                        is_empty = jump_sq_bb & UOCC;
                        if(is_empty) {
                            end = jump_sq;
                            WM = jumper_bb | jump_sq_bb;
                            BM = next_sq_bb;
                            KM = WM | (BM & K);
                            get_jumps_W(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                            same_turn = false;
                        }
                    }
                }
            }
            if(!same_turn) {
                move = Move(start,end,WP_orig^WP^WM,BP_orig^BP^BM,K_orig^K^KM);
                if(find(moves.begin(),moves.end(),move) == moves.end())
                    moves.push_back(move);
            }

            next_sq = Down_Right[jumper_num];
            if(next_sq != 99) {
                next_sq_bb = S[next_sq];
                is_opp = next_sq_bb & BP;
                if(is_opp) {
                    jump_sq = Down_Right[get_msb(next_sq_bb)];
                    if(jump_sq != 99) {
                        jump_sq_bb = S[jump_sq];
                        is_empty = jump_sq_bb & UOCC;
                        if(is_empty) {
                            end = jump_sq;
                            WM = jumper_bb | jump_sq_bb;
                            BM = next_sq_bb;
                            KM = WM | (BM & K);
                            get_jumps_W(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                            same_turn = false;
                        }
                    }
                }
            }
            if(!same_turn) {
                move = Move(start,end,WP_orig^WP^WM,BP_orig^BP^BM,K_orig^K^KM);
                if(find(moves.begin(),moves.end(),move) == moves.end())
                    moves.push_back(move);
            }
        }
    }
    void get_jumps_B(UINT jumper_num, UINT WP, UINT BP, UINT K, UINT WP_orig, UINT BP_orig, UINT K_orig, UINT start, UINT &end, vector<Move> &moves) {
        UINT WM, BM, KM;
        UINT jumper_bb = S[jumper_num];
        UINT UOCC = ~(WP|BP);
        UINT is_king = jumper_bb & K;
        UINT next_sq, next_sq_bb;
        UINT jump_sq, jump_sq_bb;
        UINT is_opp, is_empty, is_promote;
        bool same_turn = true;
        Move move;

        // Check if pieces can jump Down-Left
        next_sq = Down_Left[jumper_num];
        if(next_sq != 99) {
            next_sq_bb = S[next_sq];
            is_opp = next_sq_bb & WP;
            if(is_opp) {
                jump_sq = Down_Left[get_msb(next_sq_bb)];
                if(jump_sq != 99) {
                    jump_sq_bb = S[jump_sq];
                    is_empty = jump_sq_bb & UOCC;
                    if(is_empty) {
                        end = jump_sq;
                        BM = jumper_bb | jump_sq_bb;
                        WM = next_sq_bb;
                        KM = WM & K;
                        if(!is_king) {
                            is_promote = jump_sq_bb & MASK_BOT;
                            if(is_promote) {
                                KM |= jump_sq_bb;
                                same_turn = false;
                            }
                            else {
                                get_jumps_B(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);  // K^0 = K
                                same_turn = false;
                            }
                        }
                        else {
                            KM |= BM;
                            get_jumps_B(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                            same_turn = false;
                        }
                    }
                }
            }
        }
        if(!same_turn) {
            move = Move(start,end,WP_orig^WP^WM,BP_orig^BP^BM,K_orig^K^KM);
            if(find(moves.begin(),moves.end(),move) == moves.end())
                moves.push_back(move);
        }

        // Check if pieces can jump Down-Right
        next_sq = Down_Right[jumper_num];
        if(next_sq != 99) {
            next_sq_bb = S[next_sq];
            is_opp = next_sq_bb & WP;
            if(is_opp) {
                jump_sq = Down_Right[get_msb(next_sq_bb)];
                if(jump_sq != 99) {
                    jump_sq_bb = S[jump_sq];
                    is_empty = jump_sq_bb & UOCC;
                    if(is_empty) {
                        end = jump_sq;
                        BM = jumper_bb | jump_sq_bb;
                        WM = next_sq_bb;
                        KM = WM & K;
                        if(!is_king) {
                            is_promote = jump_sq_bb & MASK_BOT;
                            if(is_promote) {
                                KM |= jump_sq_bb;
                                same_turn = false;
                            }
                            else {
                                get_jumps_B(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                                same_turn = false;
                            }
                        }
                        else {
                            KM |= BM;
                            get_jumps_B(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                            same_turn = false;
                        }
                    }
                }
            }
        }
        if(!same_turn) {
            move = Move(start,end,WP_orig^WP^WM,BP_orig^BP^BM,K_orig^K^KM);
            if(find(moves.begin(),moves.end(),move) == moves.end())
                moves.push_back(move);
        }

        // Check if pieces are Kings and then check Up-Left and Up-Right
        if(is_king) {
            next_sq = Up_Left[jumper_num];
            if(next_sq != 99) {
                next_sq_bb = S[next_sq];
                is_opp = next_sq_bb & WP;
                if(is_opp) {
                    jump_sq = Up_Left[get_msb(next_sq_bb)];
                    if(jump_sq != 99) {
                        jump_sq_bb = S[jump_sq];
                        is_empty = jump_sq_bb & UOCC;
                        if(is_empty) {
                            end = jump_sq;
                            BM = jumper_bb | jump_sq_bb;
                            WM = next_sq_bb;
                            KM = BM | (WM & K);
                            get_jumps_B(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                            same_turn = false;
                        }
                    }
                }
            }
            if(!same_turn) {
                move = Move(start,end,WP_orig^WP^WM,BP_orig^BP^BM,K_orig^K^KM);
                if(find(moves.begin(),moves.end(),move) == moves.end())
                  moves.push_back(move);
            }

            next_sq = Up_Right[jumper_num];
            if(next_sq != 99) {
                next_sq_bb = S[next_sq];
                is_opp = next_sq_bb & WP;
                if(is_opp) {
                    jump_sq = Up_Right[get_msb(next_sq_bb)];
                    if(jump_sq != 99) {
                        jump_sq_bb = S[jump_sq];
                        is_empty = jump_sq_bb & UOCC;
                        if(is_empty) {
                            end = jump_sq;
                            BM = jumper_bb | jump_sq_bb;
                            WM = next_sq_bb;
                            KM = BM | (WM & K);
                            get_jumps_B(jump_sq,WP^WM,BP^BM,K^KM,WP_orig,BP_orig,K_orig,start,end,moves);
                            same_turn = false;
                        }
                    }
                }
            }
            if(!same_turn) {
                move = Move(start,end,WP_orig^WP^WM,BP_orig^BP^BM,K_orig^K^KM);
                if(find(moves.begin(),moves.end(),move) == moves.end())
                    moves.push_back(move);
            }
        }
    }


    //
    // GET_MOVES() - calls on the other 'get' functions to get all legal moves
    // returns false if there are no moves left
    //
    bool get_moves(UINT turn, UINT WP, UINT BP, UINT K, UINT &end,vector<Move> &moves) {
        moves.clear();
        UINT walker_num, walkers;
        UINT jumper_num, jumpers;

        if(turn == WHITE) {
            jumpers = get_jumpers_W(WP,BP,K);
            while(jumpers) { 
                jumper_num = get_lsb(jumpers);
                jumpers ^= S[jumper_num];
                get_jumps_W(jumper_num,WP,BP,K,WP,BP,K,jumper_num,end,moves);
                if(!jumpers)
                    return true;
            }
            walkers = get_walkers_W(WP,BP,K);
            while(walkers) {
                walker_num = get_lsb(walkers);
                walkers ^= S[walker_num];
                get_walks_W(walker_num,WP,BP,K,moves);
                if(!walkers)
                    return true;
            }
        }

        else if(turn == BLACK) {
            jumpers = get_jumpers_B(WP,BP,K);
            while(jumpers) {
                jumper_num = get_lsb(jumpers);
                jumpers ^= S[jumper_num];
                get_jumps_B(jumper_num,WP,BP,K,WP,BP,K,jumper_num,end,moves);
                if(!jumpers)
                    return true;
            }
            walkers = get_walkers_B(WP,BP,K);
            while(walkers) {
                walker_num = get_lsb(walkers);
                walkers ^= S[walker_num];
                get_walks_B(walker_num,WP,BP,K,moves);
                if(!walkers)
                    return true;
            }
        }

        return false;
    }


    //
    // MAKING MOVES FOR PLAYER/COMPUTER
    //
    bool player_move(UINT start, UINT end) {
        Move move, this_move = Move(start,end);
        vector<Move>::iterator itr;
        for(itr = m_moves.begin(); itr != m_moves.end(); itr++) {
            move = *itr;
            if(move == this_move) {
                UINT WP_old = m_WP;
                UINT BP_old = m_BP;
                UINT K_old = m_K;
                m_WP ^= move.WM;
                m_BP ^= move.BM;
                m_K ^= move.KM;

                cout << endl;
                cout << "You moved from " << bitnum_to_coord(start) << " to " << bitnum_to_coord(end) << "." << endl;
                if((S[start] & (WP_old | BP_old) & (~K_old)) && (S[end] & m_K)) {
                    if(m_turn == WHITE)
                        cout << "White piece at " << bitnum_to_coord(end) << " has been promoted to a King!" << endl; 
                    else if(m_turn == BLACK)
                        cout << "Black piece at " << bitnum_to_coord(end) << " has been promoted to a King!" << endl;
                }

                cout << endl;
                return true;
            }
        }
        cout << "Illegal Move!, please check the legal moves list and try again." << endl;
        cout << endl;
        return false;
    }
    void computer_move(bool is_max_node) {
        signal(SIGALRM, signalHandler);

        cpu_time_up = false;
        is_leaf_node = false;
        best_move = best_move_temp = Move(0,0,0,0,0);

        // return if there are no more moves
        if(m_moves.size() == 0)
            return;

        // If there is only one move, take it
        else if(m_moves.size() == 1) {
            cpu_maxdepth = 1;
            best_move = m_moves.back();
        }

        // If there are more than one move, search for best move
        else {
            alarm(cpu_timelimit);
            itr_deepening(is_max_node,1,INFTY_P);
            if(best_move == Move(0,0,0,0,0))
                best_move = m_moves.at(rand() % m_moves.size());
        }

        cpu_time_up = false;
        is_leaf_node = false;

        // Update board the selected move
        UINT WP_old = m_WP;
        UINT BP_old = m_BP;
        UINT K_old = m_K;
        m_WP ^= best_move.WM;
        m_BP ^= best_move.BM;
        m_K ^= best_move.KM;

        cout << endl;
        cout << "Computer moved from " << bitnum_to_coord(best_move.start) << " to " << bitnum_to_coord(best_move.end) << "." << endl;

        // If start piece is regular && end piece is a king
        if((S[best_move.start] & (WP_old | BP_old) & (~K_old)) && (S[best_move.end] & m_K)) {
            if(m_turn == WHITE) {
                cout << "White piece at " << bitnum_to_coord(best_move.end) << " has been promoted to a King!" << endl;
            }
            else if(m_turn == BLACK) {
                cout << "Black piece at " << bitnum_to_coord(best_move.end) << " has been promoted to a King!" << endl;
            }
        }

        cout << endl;
        return;
    }


    //
    // MINIMAX W/ ALPHA-BETA PRUNING
    // ITERATIVE DEEPENING
    //
    int alpha_beta_minimax(bool is_max_node, int depth, int min, int max, UINT WP, UINT BP, UINT K) {

        if(cpu_time_up)
            return is_max_node ? INFTY_P : INFTY_N;

        // depth is 0 or node is leaf, return value
        if(depth == 0) {
            int value;
            value = heuristics(WP,BP,K);
            return value;
        }

        // Get moves of current player
        vector<Move> moves;
        get_moves(is_max_node ? WHITE : BLACK, WP, BP, K, end_temp, moves);
        is_leaf_node = false;
        if(moves.empty()) {
            is_leaf_node = true;
            return  is_max_node ? INFTY_N + depth : INFTY_P - depth;
        }

        // Max function
        if(is_max_node) {
            for(int i = 0; i < moves.size(); i++) {
                Move move = moves[i];
                UINT WP_next = WP ^ move.WM;
                UINT BP_next = BP ^ move.BM;
                UINT K_next = K ^ move.KM;
                int value = alpha_beta_minimax(!is_max_node,depth-1,min,max,WP_next,BP_next,K_next);

                if(value > min) {
                    min = value;
                    if(depth == root_depth)
                        best_move_temp = move;
                }

                if(min >= max)
                    return max;
            }
        }

        // Min function
        else {
            for(int i = 0; i < moves.size(); i++) {
                Move move = moves[i];
                UINT WP_next = WP ^ move.WM;
                UINT BP_next = BP ^ move.BM;
                UINT K_next = K ^ move.KM;
                int value = alpha_beta_minimax(!is_max_node,depth-1,min,max,WP_next,BP_next,K_next);

                if(value < max) {
                    max = value;
                    if(depth == root_depth)
                        best_move_temp = move;
                }

                if(max <= min)
                    return min;
            }
        }

        return is_max_node ? min : max;
    }
    void itr_deepening(bool is_max_node, int start_depth, int end_depth) {

        // Begin search
        // cout << "MiniMax Iterative deepening in progress..." << endl;
        int depth;
        for(depth = start_depth; depth <= end_depth; depth++) {
            root_depth = depth;
            alpha_beta_minimax(is_max_node,depth,INFTY_N,INFTY_P,m_WP,m_BP,m_K);

            if(cpu_time_up) {
                // cout << "CPU time limit for searching was reached." << endl;
                break;
            }
            else {
                best_move = best_move_temp;
                cpu_maxdepth = depth;
            }

            if(is_leaf_node)
                break;
        }
    }
    

    //
    // HEURISTICS FUNCTION
    //
    int heuristics(UINT WP, UINT BP, UINT K) {
        if(!WP) return INFTY_N;
        if(!BP) return INFTY_P;
        int return_value = 0;
        int offset = 1e4;
        UINT WPawns = WP&(~K), WK = WP&K;
        UINT BPawns = BP&(~K), BK = BP&K;

        // Number of each piece on board
        int w_pawn_count = get_bit_count(WPawns);
        int b_pawn_count = get_bit_count(BPawns);
        int w_king_count = get_bit_count(WK);
        int b_king_count = get_bit_count(BK);
        int white_count = w_pawn_count + 1.5*w_king_count;
        int black_count = b_pawn_count + 1.5*b_king_count;

        // Pieces on their first row have more weight
        UINT Wfirst = WPawns & MASK_BOT;
        UINT Bfirst = BPawns & MASK_TOP;
             
        // Get jumpers
        UINT Wjump = get_jumpers_W(WP,BP,K);
        UINT Bjump = get_jumpers_B(WP,BP,K);

        // Edges are discouraged for kings
        if(WK & MASK_EDGES) return_value -= offset*10;
        if(BK & MASK_EDGES) return_value += offset*10;

        // Loop through each square
        UINT square;
        for(UINT i = 0; i < 32; i++) {
            square = S[i];

            // On white-starting side of board
            if(i > 19) {
                // Increase score for each white piece on white-starting side
                if(square & WPawns) return_value += offset*2000;
                // Increase score for each white piece on its first row
                if(square & Wfirst) return_value += offset*10;
                // Decrease score for each black piece on white-starting side
                if(square & BPawns) return_value -= offset*2010;
            }

            // On black-starting side of board
            else if(i < 12) {
                if(square & BPawns) return_value -= offset*2000;
                if(square & Bfirst) return_value -= offset*10;
                if(square & WPawns) return_value += offset*2010;
            }

            // In neutral region, increase score for each piece
            else {
                if(square & WPawns) return_value += offset*2000;
                if(square & BPawns) return_value -= offset*2000;
            }

            // Points for Kings
            if(square & WK) return_value += offset*3000;
            if(square & BK) return_value -= offset*3000;

            // Pieces that can jump
            if(square & Wjump) return_value += offset*100;
            if(square & Bjump) return_value -= offset*100;
        }

        // When both players have less than 6 pieces (pawns count as 1, kings count as 1.5),
        // Winning player will be more aggressive
        // Losing player will be more defensive
        if(white_count < 6 && black_count < 6) {
            if(white_count > black_count) {
                // Losing player get more points for double corners
                if(BP & (MASK_DBLCORNER1 | MASK_DBLCORNER2))
                    return_value -= offset*100;

                // Winning player focused more on capturing
                return_value -= offset*b_pawn_count*300;
                return_value -= offset*b_king_count*500;
            }

            else if(white_count < black_count) {
                if(WP & (MASK_DBLCORNER1 | MASK_DBLCORNER2))
                    return_value += offset*100;
                return_value += offset*w_pawn_count*300;
                return_value += offset*w_king_count*500;
            }
        }

        // Add a slight randomness to the value
        return_value += ((rand() % 2001) - 1000);
        return return_value;
    }


    //
    // PRINT FUNCTIONS
    //

    // helper functions to convert between coordinates and bitnums
    string bitnum_to_coord(UINT square_num) {
        int r = square_num / 4;
        int c = (square_num % 4) * 2;
        if(r % 2 == 0)
            c++;
        int row = r + 1;
        char col = char(c + 97);
        stringstream ss;
        ss << "(" << row << "," << col << ")";
        return ss.str();
    }
    UINT coord_to_bitnum(int row, char col) {
        int r = row - 1;
        int c = int(col - 97);
        if((r % 2 == 0 && c % 2 == 0) || (r % 2 == 1 && c % 2  == 1))
            return 33;
        if(r % 2 == 0)
            c--;
        c /= 2;
        return  r * 4 + c;
    }

    void print_board(UINT WP, UINT BP, UINT K) {
        cout << "\033[0m"; // return to default color for console

        ios::fmtflags f = cout.flags();
        cout << "\e[47m"
             << "\e[1;31m" << "  Black:" << setw(2) << get_bit_count(m_BP)
             << "\e[1;34m" << "   White:" << setw(2) << get_bit_count(m_WP) << "  "
             << "\e[0m" << endl
             << "\e[47m" << "                       " << "\e[0m" << endl;
        cout.flags(f);

        // Column letters (top)
        cout << "\e[1;30;47m" << "     a b c d e f g h   " << "\e[0m" << endl
             << "\e[1;30;47m" << "                       " << "\e[0m" << endl;

        // i goes through pieces
        // toggle determines format of empty square
        // counter determines next line
        UINT i, toggle = 0, counter = 0, row = 1;

        for(i = 0; i < 32; i++) {

            // ANSI escape sequence format
            // "\e[n;n;nm"
            // n represents a trait, bold, color, etc

            if(counter == 0)
                cout << "\e[47m" << "\e[1;30m" << row << "   " << "|";

            if(toggle == 0)
                cout << "\e[47m" << " " << "\e[1;30m" << "|";

            if(WP & ~K & (1 << i))
                cout << "\e[0m" << "\e[34;47m" << "w" << "\e[1;30m" << "|";
            else if(WP & K & (1 << i))
                cout << "\e[0m" << "\e[1;34;47m" << "W" << "\e[1;30m" << "|";
            else if(BP & ~K & (1 << i))
                cout << "\e[0m" << "\e[31;47m" << "b" << "\e[1;30m" << "|";
            else if(BP & K & (1 << i))
                cout << "\e[0m" << "\e[1;31;47m" << "B" << "\e[1;30m" << "|";
            else
                cout << "\e[47m" << " " << "\e[1;30m" <<  "|";


            if(toggle == 1)
                cout << "\e[47m" << " " << "\e[1;30m" << "|";


            // Newline after 4 pieces
            counter = (counter + 1) % 4;
            if(counter == 0) {
                toggle ^= 1;
                cout << "\e[47m" << "  " << "\e[0m" << endl;
                row++;
            }
        }
    }

    void print_legal_moves(vector<Move> moves) {
        cout << "Legal moves: " << endl;

        UINT start, end;
        Move move;
        vector<Move>::iterator itr;
        for(itr = moves.begin(); itr != moves.end(); ++itr) {
            move = *itr;
            start = move.start;
            end = move.end;
            cout << bitnum_to_coord(start) << " => " << bitnum_to_coord(end) << endl;
        }
    }

    void print_turn_info(UINT turn, int turn_num) {
        cout << "Turn " << turn_num << ": ";
        if(turn == WHITE)
            cout << "White's turn." << endl;
        else if(turn == BLACK)
            cout << "Black's turn." << endl;
    }

    void print_winner(UINT WP, UINT BP) {
        cout << "No more possible moves left. The game is over!" << endl;
        if(get_bit_count(WP) > get_bit_count(BP))
            cout << "WHITE WINS!" << endl;
        else if(get_bit_count(WP) < get_bit_count(BP))
            cout << "BLACK WINS!" << endl;
        else if(get_bit_count(WP) == 1 && get_bit_count(BP) == 1)
            cout << "Both sides only have one piece left. DRAW!" << endl;
        else
            cout << "DRAW!" << endl;
    }

    void print_cpu_stats() {
        streamsize ss = cout.precision();
        cout << "CPU search time: " << fixed << setprecision(3) << cpu_time << endl;
        cout.precision(ss);
        cout << "Max depth searched: " << cpu_maxdepth << endl;
    }


    //
    // SETUP_PARAMETERS() AND PLAY()
    //
    void setup_parameters() {
        cpu_time = 0;
        cpu_timelimit = 0;
        cpu_maxdepth = 0;
        root_depth=  0;
        cpu_time_up = false;
        m_WP = 0;
        m_BP = 0;
        m_K = 0;
        end_temp = 0;
        best_move = best_move_temp = Move(0,0,0,0,0);
        m_moves.clear();
        m_turn = 0;
        m_turn_num = 1;
    }

    int play() {
        // player inputs and game states
        int row1, row2;
        char col1, col2;
        UINT start, end;
        char play_again;

        cout << endl;
        cout << "~~~~~ Welcome to Checkers! ~~~~~" << endl;
        cout << endl;

        do {
            setup_parameters();
            int mode;
            char exist_board;

            while(true) {
                cout << "Game modes available:" << endl
                  << "1 - Human    VS. Human" << endl
                  << "2 - Human    VS. Computer" << endl
                  << "3 - Computer VS. Computer" << endl;
                cout << "Please select a mode by number: ";
                if(!(cin >> mode)) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "Error: Invalid input, please try again." << endl;
                }
                else if(mode != 1 && mode != 2 && mode != 3)
                    cout << "Error: Please enter 1, 2, or 3." << endl;
                else {
                    cout << endl;
                    break;
                }
                cout << endl;
            }

            switch(mode) {
                case 1:
                    White_Player = HUMAN;
                    BlacK_Player = HUMAN;
                    break;
                case 2:
                    char wb;
                    while(true) {
                        cout << "Would you like to play as white or black (w/b)? ";
                        if(!(cin >> wb)) {
                            cin.clear();
                            cin.ignore(numeric_limits<streamsize>::max(), '\n');
                            cout << "Error: Invalid input, please try again." << endl;
                        }
                        else if(wb != 'w' && wb != 'b')
                            cout << "Error: You did not enter w or b." << endl;
                        else {
                            cout << endl;
                            break;
                        }
                    }
                    if(wb == 'w') {
                        White_Player = HUMAN;
                        BlacK_Player = COMPUTER;
                        cout << "You are white. The computer will play as black." << endl;
                    }
                    else if(wb == 'b') {
                        BlacK_Player = HUMAN;
                        White_Player = COMPUTER;
                        cout << "You are black. The computer will play as white." << endl;
                    }
                    cout << endl;
                    break;
                case 3:
                    White_Player = COMPUTER;
                    BlacK_Player = COMPUTER;
                    break;
            }


            while(true) {
                cout << "Would you like to load an existing board (y/n)? ";
                if(!(cin >> exist_board)) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "Error: Invalid input, please try again." << endl;
                }
                else if(exist_board != 'y' && exist_board != 'n')
                    cout << "Error: You did not enter y or n." << endl;
                else {
                    cout << endl;
                    break;
                }
            }

            if(exist_board == 'y')
                cust_board(m_WP,m_BP,m_K,m_turn,cpu_timelimit);

            else {
                init_board(m_WP,m_BP,m_K);
                while(true) {
                    cout << "Pick starting player (0 for white, 1 for black): ";
                    if(!(cin >> m_turn)) {
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        cout << "Error: Invalid input, please try again." << endl;
                    }
                    else if(m_turn != 0 && m_turn != 1)
                        cout << "Error: You entered an number other than 0 or 1." << endl;
                    else {
                        cout << endl;
                        break;
                    }
                }

                //Select CPU time limit
                if(mode == 2 || mode == 3) {
                    while(true){
                        cout << "Designate time limit for CPU (integers only): ";
                        if (!(cin >> cpu_timelimit)) {
                            cin.clear();
                            cin.ignore(numeric_limits<streamsize>::max(), '\n');
                            cout << "Error: Invalid input, please try again." << endl;
                        }
                        else if (cpu_timelimit <= 0)
                            cout << "Error: You entered a negative number" << endl;
                        else {
                            cout << endl;
                            break;
                        }
                    }
                }
            }


            //Run game
            while(get_moves(m_turn,m_WP,m_BP,m_K,end_temp,m_moves)) {

                cout << endl << endl;
                cout << "~~~~~~~~~~~~~~~~~~~~~~" << endl;
                print_turn_info(m_turn,m_turn_num);
                cout << "~~~~~~~~~~~~~~~~~~~~~~" << endl;

                // Player is HUMAN
                if((m_turn == WHITE && White_Player == HUMAN) || (m_turn == BLACK && BlacK_Player == HUMAN)) {

                    do {
                        print_legal_moves(m_moves);
                        cout << endl;
                        print_board(m_WP,m_BP,m_K);
                        cout << endl;

                        while(cout << "Specify move <from> <to> (ex. '6e 5f'): " && !(cin >> row1 >> col1 >> row2 >> col2)) {
                            cin.clear(); //clear bad input flag
                            cin.ignore(numeric_limits<streamsize>::max(), '\n'); //discard input
                            cout << "Error: Invalid input, please try again." << endl;
                        }

                        start = coord_to_bitnum(row1,col1);
                        end = coord_to_bitnum(row2,col2);
                    } while(!player_move(start,end));
                }

                // Player is COMPUTER
                else {
                    print_legal_moves(m_moves);
                    cout << endl;
                    print_board(m_WP,m_BP,m_K);
                    cout << endl;

                    clock_t t1, t2, t1_m, t2_m;
                    cout << "Computer is thinking..." << endl;

                    t1_m = clock();
                    computer_move(m_turn == WHITE ? true : false);
                    t2_m = clock();

                    cpu_time = double(t2_m-t1_m)/CLOCKS_PER_SEC;
                    print_cpu_stats();
                }

                m_turn ^= 1;
                m_turn_num++;
            }

            //Display winner
            cout << endl << endl;
            print_board(m_WP,m_BP,m_K);
            cout << endl;
            cout << "+~+~+~+~+~+~+~+~+~+~+~+~+~+~+" << endl;
            print_winner(m_WP,m_BP);

            //Prompt to play again
            while(true) {
                cout << "Play again (y/n)? ";
                if(!(cin >> play_again)) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "Error: Invalid input, please try again." << endl;
                }
                else if(play_again != 'y' && play_again != 'n')
                    cout << "Error: You did not enter y or n." << endl;
                else {
                    cout << endl;
                    break;
                }
            }
        } while(play_again == 'y');

        return 0;
    }
};


int main() {
    Game CheckersAI_Demo= Game();
    CheckersAI_Demo.play();
    return 0;
}