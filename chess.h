#include <iostream>
#include <cstring>
#include <stdint.h>
#include <assert.h>
#include "short_list.h"





//--------------------------------
typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef unsigned long long u64;

typedef i16 depth_t;
typedef u8 piece_t;
typedef u8 coord_t;

#define ONBRD(X)        (!((X) & 0x88))
#define XY2SQ(X, Y)     (((Y) << 4) + (X))
#define COL(SQ)         ((SQ) & 7)
#define ROW(SQ)         ((SQ) >> 4)
#define ABSI(X)         ((X) > 0 ? (X) : (-(X)))
#define GET_INDEX(X)    ((X)/2)
#define TO_BLACK(X)     ((X) & ~white)

const i32 lst_sz  = 32;                                                 // size of piece list for one colour
const depth_t max_ply  = 100;                                                // maximum search depth
const depth_t prev_states   = 4;
const piece_t white = 1;
const piece_t black = 0;

typedef short_list<coord_t, lst_sz>::iterator_entity iterator_entity;
typedef short_list<coord_t, lst_sz>::iterator iterator;
typedef i16 score_t;
typedef i16 streng_t;
typedef u8 side_to_move_t;
typedef u8 move_score_t;
typedef i16 tropism_t;
typedef u8 move_flag_t;
typedef u8 piece_index_t;
typedef u8 castle_t;
typedef u8 enpass_t;
typedef u8 attack_t;
typedef u8 deltas_t;
typedef i8 shifts_t;
typedef u64 hash_key_t;
typedef u64 node_t;
typedef u32 history_t;
typedef u8 piece_num_t;




//--------------------------------
enum {__ = 0,  _k = 2, _q = 4, _r = 6, _b = 8, _n = 10, _p = 12,
               _K = 3, _Q = 5, _R = 7, _B = 9, _N = 11, _P = 13};
enum {mCAPT = 0x10, mCS_K = 0x20, mCS_Q = 0x40, mCSTL = 0x60, mENPS = 0x80,
      mPR_Q = 0x01, mPR_N = 0x02, mPR_R = 0x03, mPR_B = 0x04, mPROM = 0x07};





//--------------------------------
class Move
{
public:
    coord_t to;
    iterator_entity pc;
    move_flag_t flg;
    move_score_t scr;

    bool operator == (Move m)   {return to == m.to && pc == m.pc && flg == m.flg;}
    bool operator != (Move m)   {return to != m.to || pc != m.pc || flg != m.flg;}
    bool operator < (const Move m) const {return scr < m.scr;}
};
                                                                        // 'to' - coords for piece to move to (8 bit);
                                                                        // 'pc' - number of piece in 'men' array (8 bit);
                                                                        // 'flg' - flags (mCAPT, etc) - (8 bit)
                                                                        // 'scr' - unsigned score (priority) by move generator (8 bit)
                                                                        //      0..63    - bad captures
                                                                        //      64..127  - silent moves without history (pst value)
                                                                        //      128..195 - silent moves with history value
                                                                        //      196 - equal capture
                                                                        //      197 - move from pv
                                                                        //      198 - second killer
                                                                        //      199 - first killer
                                                                        //      200..250 - good captures and/or promotions
                                                                        //      255 - opp king capture or hash hit

//--------------------------------
struct BrdState
{
    piece_t capt;                                                            // taken piece, 6 bits
    iterator_entity captured_it;                                        // iterator to captured piece
    coord_t fr;                                                         // from point, 7 bits
    castle_t cstl;                                                      // castling rights, bits 0..3: _K, _Q, _k, _q, 4 bits

    iterator_entity castled_rook_it;                                    // iterator to castled rook, 8 bits
    enpass_t ep;                                                        // 0 = no_ep, else ep=COL(x) + 1, not null only if opponent pawn is near, 4 bits
    iterator_entity nprom;                                              // number of next piece for promoted pawn, 6 bits
    depth_t reversibleCr;                                                   // reversible halfmove counter
    coord_t to;                                                              // to point, 7 bits (for simple repetition draw detection)
    score_t val_opn;                                                    // store material and PST value considered all material is on the board
    score_t val_end;                                                    // store material and PST value considered deep endgame (kings and pawns only)
    move_score_t scr;                                                             // move priority by move genererator
    tropism_t tropism[2];
};





//--------------------------------
void InitChess();
void InitAttacks();
void InitBrd();
bool BoardToMen();
bool FenToBoard(char *p);
void ShowMove(coord_t fr, coord_t to);
bool MakeCastle(Move m, coord_t fr);
void UnMakeCastle(Move m);
bool MakeEP(Move m, coord_t fr);
bool SliderAttack(coord_t fr, coord_t to);
bool Attack(coord_t to, side_to_move_t xtm);
bool LegalSlider(coord_t fr, coord_t to, piece_t pt);
bool Legal(Move m, bool ic);
void SetPawnStruct(coord_t col);
void InitPawnStruct();
bool PieceListCompare(coord_t men1, coord_t men2);
void StoreCurrentBoardState(Move m, coord_t fr, coord_t targ);
void MakeCapture(Move m, coord_t targ);
void MakePromotion(Move m, iterator it);
void UnmakeCapture(Move m);
void UnmakePromotion(Move m);
bool MkMoveFast(Move m);
void UnMoveFast(Move m);
