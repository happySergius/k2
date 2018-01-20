#include "eval.h"





//-----------------------------
k2chess::eval_t k2eval::Eval()
{
    state[ply].val_opn = val_opn;
    state[ply].val_end = val_end;

    EvalPawns(white);
    EvalPawns(black);

    KingSafety(white);
    KingSafety(black);

    MaterialImbalances();

    auto ans = -ReturnEval(wtm);
    ans -= 8;  // bonus for side to move

    val_opn = state[ply].val_opn;
    val_end = state[ply].val_end;

    return ans;
}





//-----------------------------
void k2eval::FastEval(const move_c m)
{
    eval_t ansO = 0, ansE = 0;

    auto x = get_col(m.to_coord);
    auto y = get_row(m.to_coord);
    auto x0 = get_col(k2chess::state[ply].from_coord);
    auto y0 = get_row(k2chess::state[ply].from_coord);
    auto to_type = get_type(b[m.to_coord]);
    if(!wtm)
    {
        y = 7 - y;
        y0 = 7 - y0;
    }

    coord_t type;
    auto flag = m.flag & is_promotion;
    if(flag)
    {
        type = pawn;
        ansO -= material_values_opn[type] + pst[type - 1][opening][y0][x0];
        ansE -= material_values_end[type] + pst[type - 1][endgame][y0][x0];

        piece_t promo_pieces[] =
        {
            black_queen, black_knight, black_rook, black_bishop
        };
        type = get_type(promo_pieces[flag - 1]);
        ansO += material_values_opn[type] + pst[type - 1][opening][y0][x0];
        ansE += material_values_end[type] + pst[type - 1][endgame][y0][x0];

    }

    if(m.flag & is_capture)
    {
        auto captured_piece = k2chess::state[ply].captured_piece;
        if(m.flag & is_en_passant)
        {
            type = pawn;
            ansO += material_values_opn[type] +
                    pst[type - 1][opening][7 - y0][x];
            ansE += material_values_end[type] +
                    pst[type - 1][endgame][7 - y0][x];
        }
        else
        {
            type = get_type(captured_piece);
            ansO += material_values_opn[type] +
                    pst[type - 1][opening][7 - y][x];
            ansE += material_values_end[type] +
                    pst[type - 1][endgame][7 - y][x];
        }
    }
    else if(m.flag & is_right_castle)
    {
        type = rook;
        auto rook_col = default_king_col + cstl_move_length - 1;
        ansO += pst[type - 1][opening][max_row][rook_col] -
                pst[type - 1][opening][max_row][max_col];
        ansE += pst[type - 1][endgame][max_row][rook_col] -
                pst[type - 1][endgame][max_row][max_col];
    }
    else if(m.flag & is_left_castle)
    {
        type = rook;
        auto rook_col = default_king_col - cstl_move_length + 1;
        ansO += pst[type - 1][opening][max_row][rook_col] -
                pst[type - 1][opening][max_row][0];
        ansE += pst[type - 1][endgame][max_row][rook_col] -
                pst[type - 1][endgame][max_row][0];
    }

    ansO += pst[to_type - 1][opening][y][x] -
            pst[to_type - 1][opening][y0][x0];
    ansE += pst[to_type - 1][endgame][y][x] -
            pst[to_type - 1][endgame][y0][x0];

    val_opn += wtm ? -ansO : ansO;
    val_end += wtm ? -ansE : ansE;
}





//-----------------------------
void k2eval::InitEvalOfMaterialAndPst()
{
    val_opn = 0;
    val_end = 0;
    for(auto col = 0; col <= max_col; ++col)
        for(auto row = 0; row <= max_row; ++row)
        {
            auto piece = b[get_coord(col, row)];
            if(piece == empty_square)
                continue;
            auto row_ = row;
            if(piece & white)
                row_ = 7 - row;

            auto type = get_type(piece);
            auto delta_o = material_values_opn[type] +
                    pst[type - 1][opening][row_][col];
            auto delta_e = material_values_end[type] +
                    pst[type - 1][endgame][row_][col];

            if(piece & white)
            {
                val_opn += delta_o;
                val_end += delta_e;
            }
            else
            {
                val_opn -= delta_o;
                val_end -= delta_e;
            }
    }
    state[ply].val_opn = val_opn;
    state[ply].val_end = val_end;

    king_tropism[white] = CountKingTropism(white);
    king_tropism[black] = CountKingTropism(black);
}





//--------------------------------
bool k2eval::IsPasser(const coord_t col, const bool stm) const
{
    auto mx = pawn_max[col][stm];

    if(mx >= max_row - pawn_min[col][!stm]
            && mx >= max_row - pawn_min[col - 1][!stm]
            && mx >= max_row - pawn_min[col + 1][!stm])
        return true;
    else
        return false;
}





//--------------------------------
void k2eval::EvalPawns(const bool stm)
{
    eval_t ansO = 0, ansE = 0;
    bool passer, prev_passer = false;
    bool opp_only_pawns = material[!stm]/100 == pieces[!stm] - 1;

    for(auto col = 0; col <= max_col; col++)
    {
        bool doubled = false, isolany = false;

        auto mx = pawn_max[col][stm];
        if(mx == 0)
        {
            prev_passer = false;
            continue;
        }

        if(pawn_min[col][stm] != mx)
            doubled = true;
        if(pawn_max[col - 1][stm] == 0 && pawn_max[col + 1][stm] == 0)
            isolany = true;
        if(doubled && isolany)
        {
            ansE += pawn_dbl_iso_end;
            ansO += pawn_dbl_iso_opn;
        }
        else if(isolany)
        {
            ansE += pawn_iso_end;
            ansO += pawn_iso_opn;
        }
        else if(doubled)
        {
            ansE += pawn_dbl_end;
            ansO += pawn_dbl_opn;
        }

        passer = IsPasser(col, stm);
        if(!passer)
        {
            // pawn holes occupied by enemy pieces
            if(col > 0 && col < max_col && mx < pawn_min[col - 1][stm]
                    && mx < pawn_min[col + 1][stm])
            {
                auto y_coord = stm ? mx + 1 : max_row - mx - 1;
                auto op_piece = b[get_coord(col, y_coord)];
                bool occupied = is_dark(op_piece, stm)
                                && get_type(op_piece) != pawn;
                if(occupied)
                {
                    ansE += pawn_hole_end;
                    ansO += pawn_hole_opn;
                }
            }
            // gaps in pawn structure
            if(pawn_max[col - 1][stm]
                    && std::abs(mx - pawn_max[col - 1][stm]) > 1)
            {
                ansE += pawn_gap_end;
                ansO += pawn_gap_opn;
            }
            prev_passer = false;
            continue;
        }
        // following code executed only for passers

        // king pawn tropism
        auto k = *king_coord[stm];
        auto opp_k = *king_coord[!stm];
        auto pawn_coord = get_coord(col, stm ? mx + 1 : max_row - mx - 1);
        auto k_dist = king_dist(k, pawn_coord);
        auto opp_k_dist = king_dist(opp_k, pawn_coord);

        if(k_dist <= 1)
            ansE += pawn_king_tropism1 + pawn_king_tropism2*mx;
        else if(k_dist == 2)
            ansE += pawn_king_tropism3;
        if(opp_k_dist <= 1)
            ansE -= pawn_king_tropism1 + pawn_king_tropism2*mx;
        else if(opp_k_dist == 2)
            ansE -= pawn_king_tropism3;

        // passed pawn evaluation
        eval_t pass[] = {0, pawn_pass_1, pawn_pass_2, pawn_pass_3,
                         pawn_pass_4, pawn_pass_5, pawn_pass_6};
        eval_t blocked_pass[] = {0, pawn_blk_pass_1, pawn_blk_pass_2,
                                 pawn_blk_pass_3, pawn_blk_pass_4,
                                 pawn_blk_pass_5, pawn_blk_pass_6};
        auto next_square = get_coord(col, stm ? mx + 1 : max_row - mx - 1);
        bool blocked = b[next_square] != empty_square;
        auto delta = blocked ? blocked_pass[mx] : pass[mx];

        ansE += delta;
        ansO += delta/pawn_pass_opn_divider;

        // connected passers
        if(passer && prev_passer
                && std::abs(mx - pawn_max[col - 1][stm]) <= 1)
        {
            auto mmx = std::max(pawn_max[col - 1][stm], mx);
            if(mmx > 4)
                ansE += pawn_pass_connected*mmx;
        }
        prev_passer = true;

        // unstoppable
        if(opp_only_pawns && IsUnstoppablePawn(col, stm, wtm))
        {
            ansO += pawn_unstoppable_1*mx + pawn_unstoppable_2;
            ansE += pawn_unstoppable_1*mx + pawn_unstoppable_2;
        }
    }
    val_opn += stm ? ansO : -ansO;
    val_end += stm ? ansE : -ansE;
}





//-----------------------------
bool k2eval::IsUnstoppablePawn(const coord_t col, const bool side_of_pawn,
                               const bool stm) const
{
    auto pmax = pawn_max[col][side_of_pawn];
    if(pmax == pawn_default_row)
        pmax++;
    auto promo_square = get_coord(col, side_of_pawn ? max_row : 0);
    int dist = king_dist(*king_coord[!side_of_pawn], promo_square);
    auto k = *king_coord[side_of_pawn];
    if(get_col(k) == col && king_dist(k, promo_square) <= max_row - pmax)
        pmax--;
    return dist - (side_of_pawn  != stm) > max_row - pmax;
}





//-----------------------------
void k2eval::MaterialImbalances()
{
    auto X = material[black]/100 + 1 + material[white]/100 + 1
             - pieces[black] - pieces[white];

    if(X == 3 && (material[black]/100 == 4 || material[white]/100 == 4))
    {
        // KNk, KBk, Kkn, Kkb
        if(pieces[black] + pieces[white] == 3)
        {
            val_opn = 0;
            val_end = 0;
            return;
        }
        // KPkn, KPkb
        if(material[white]/100 == 1 && material[black]/100 == 4)
            val_end += bishop_val_end + pawn_val_end/4;
        // KNkp, KBkp
        if(material[black]/100 == 1 && material[white]/100 == 4)
            val_end -= bishop_val_end + pawn_val_end/4;
    }
    // KNNk, KBNk, KBBk, etc
    else if(X == 6 && (material[0]/100 == 0 || material[1]/100 == 0))
    {
        if(quantity[white][knight] == 2
                || quantity[black][knight] == 2)
        {
            val_opn = 0;
            val_end = 0;
            return;
        }
        // many code for mating with only bishop and knight
        else if((quantity[white][knight] == 1
                 && quantity[white][bishop] == 1)
                || (quantity[black][knight] == 1
                    && quantity[black][bishop] == 1))
        {
            auto stm = quantity[white][knight] == 1
                       ? white : black;
            auto rit = coords[stm].begin();
            for(; rit != coords[stm].end(); ++rit)
                if(get_type(b[*rit]) == bishop)
                    break;
            assert(get_type(b[*rit]) == bishop);

            eval_t ans = 0;
            auto ok = *king_coord[!stm];
            if(ok == 0x06 || ok == 0x07 || ok == 0x17
                    || ok == 0x70 || ok == 0x71 || ok == 0x60)
                ans = 200;
            if(ok == 0x00 || ok == 0x01 || ok == 0x10
                    || ok == 0x77 || ok == 0x76 || ok == 0x67)
                ans = -200;

            bool bishop_on_light_square = ((get_col(*rit)) + get_row(*rit)) & 1;
            if(!bishop_on_light_square)
                ans = -ans;
            if(!stm)
                ans = -ans;
            val_end += ans;
        }
    }
    else if(X == 3)
    {
        // KBPk or KNPk with pawn at a(h) file
        if(material[white] == 0)
        {
            auto k = *king_coord[white];
            if((pawn_max[0][black] != 0 && king_dist(k, 0) <= 1)
                    || (pawn_max[max_col][black] != 0
                        && king_dist(k, max_col) <= 1))
                val_end += 750;
        }
        else if(material[black] == 0)
        {
            auto k = *king_coord[black];
            const auto row_max = get_coord(0, max_row);
            const auto max_all = get_coord(max_col, max_row);
            if((pawn_max[0][white] != 0 && king_dist(k, row_max) <= 1)
                    || (pawn_max[max_col][white] != 0
                        && king_dist(k, max_all) <= 1))
                val_end -= 750;
        }
    }
    else if(X == 0)
    {
        // KPk
        if(material[white]/100 + material[black]/100 == 1)
        {
            bool stm = material[white]/100 == 1;
            auto it = coords[stm].rbegin();
            ++it;
            auto colp = get_col(*it);
            bool unstop = IsUnstoppablePawn(colp, stm, wtm);
            auto dist_k = king_dist(*king_coord[stm], *it);
            auto dist_opp_k = king_dist(*king_coord[!stm], *it);

            if(!unstop && dist_k > dist_opp_k + (wtm == stm))
            {
                val_opn = 0;
                val_end = 0;
            }
            else if((colp == 0 || colp == 7)
                    && king_dist(*king_coord[!stm],
                                 (colp + (stm ? 0x70 : 0))) <= 1)
            {
                val_opn = 0;
                val_end = 0;
            }
        }
    }

    // two bishops
    if(quantity[white][bishop] == 2)
    {
        val_opn += 30;
        val_end += 30;
    }
    if(quantity[black][bishop] == 2)
    {
        val_opn -= 30;
        val_end -= 30;
    }

    // pawn absence for both sides
    if(quantity[white][pawn] == 0
            && quantity[black][pawn] == 0
            && material[white] != 0 && material[black] != 0)
        val_end /= 3;

    // multicolored bishops
    if(quantity[white][bishop] == 1
            && quantity[black][bishop] == 1)
    {
        auto w_it = coords[white].rbegin();
        while(w_it != coords[white].rend()
                && b[*w_it] != white_bishop)
            ++w_it;
        assert(w_it != coords[white].rend());

        auto b_it = coords[black].rbegin();
        while(b_it != coords[black].rend()
                && b[*b_it] != black_bishop)
            ++b_it;
        assert(b_it != coords[white].rend());

        auto sum_coord_w = get_col(*w_it) + get_row(*w_it);
        auto sum_coord_b = get_col(*b_it) + get_row(*b_it);

        if((sum_coord_w & 1) != (sum_coord_b & 1))
        {
            if(material[white]/100 - pieces[white] == 4 - 2
                    && material[black]/100 - pieces[black] == 4 - 2)
                val_end /= 2;
            else
                val_end = val_end*4/5;

        }
    }

}





//-----------------------------
k2chess::eval_t k2eval::EvalDebug()
{
    state[ply].val_opn = val_opn;
    state[ply].val_end = val_end;

    auto store_vo = val_opn;
    auto store_ve = val_end;
    auto store_sum = ReturnEval(white);
    std::cout << "\t\t\tMidgame\tEndgame\tTotal" << std::endl;
    std::cout << "Material + PST\t\t";
    std::cout << val_opn << '\t' << val_end << '\t'
              << store_sum << std::endl;

    EvalPawns(white);
    std::cout << "White pawns\t\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);
    EvalPawns(black);
    std::cout << "Black pawns\t\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);

    KingSafety(white);
    std::cout << "King safety white\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);
    KingSafety(black);
    std::cout << "King safety black\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);

    MaterialImbalances();
    std::cout << "Imbalances summary\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);

    auto ans = -ReturnEval(wtm);
    ans -= 8;  // bonus for side to move
    std::cout << "Bonus for side to move\t\t\t";
    std::cout << (wtm ? 8 : -8) << std::endl << std::endl;

    std::cout << std::endl << std::endl;

    std::cout << "Eval summary: " << (wtm ? -ans : ans) << std::endl;
    std::cout << "(positive values means advantage for white)" << std::endl;

    val_opn = state[ply].val_opn;
    val_end = state[ply].val_end;

    return ans;
}





//-----------------------------
void k2eval::SetPawnStruct(const coord_t col)
{
    assert(col <= max_col);
    coord_t y;
    if(wtm)
    {
        y = pawn_default_row;
        while(b[get_coord(col, max_row - y)] != black_pawn && y < max_row)
            y++;
        pawn_min[col][black] = y;

        y = max_row - pawn_default_row;
        while(b[get_coord(col, max_row - y)] != black_pawn && y > 0)
            y--;
        pawn_max[col][black] = y;
    }
    else
    {
        y = pawn_default_row;
        while(b[get_coord(col, y)] != white_pawn && y < max_row)
            y++;
        pawn_min[col][white] = y;

        y = max_row - pawn_default_row;
        while(b[get_coord(col, y)] != white_pawn && y > 0)
            y--;
        pawn_max[col][white] = y;
    }
}





//-----------------------------
void k2eval::MovePawnStruct(const piece_t moved_piece,
                            const coord_t from_coord,
                            const move_c move)
{
    if(get_type(moved_piece) == pawn || (move.flag & is_promotion))
    {
        SetPawnStruct(get_col(move.to_coord));
        if(move.flag)
            SetPawnStruct(get_col(from_coord));
    }
    if(get_type(k2chess::state[ply].captured_piece) == pawn
            || (move.flag & is_en_passant))  // is_en_passant not needed
    {
        wtm ^= white;
        SetPawnStruct(get_col(move.to_coord));
        wtm ^= white;
    }
#ifndef NDEBUG
    rank_t copy_max[8][2], copy_min[8][2];
    memcpy(copy_max, pawn_max, sizeof(copy_max));
    memcpy(copy_min, pawn_min, sizeof(copy_min));
    InitPawnStruct();
    assert(!memcmp(copy_max, pawn_max, sizeof(copy_max)));
    assert(!memcmp(copy_min, pawn_min, sizeof(copy_min)));
#endif
}





//-----------------------------
void k2eval::InitPawnStruct()
{
    for(auto col = 0; col <= max_col; col++)
    {
        pawn_max[col][black] = 0;
        pawn_max[col][white] = 0;
        pawn_min[col][black] = max_row;
        pawn_min[col][white] = max_row;
        for(auto row = pawn_default_row;
            row <= max_row - pawn_default_row;
            row++)
            if(b[get_coord(col, row)] == white_pawn)
            {
                pawn_min[col][white] = row;
                break;
            }
        for(auto row = max_row - pawn_default_row;
            row >= pawn_default_row;
            row--)
            if(b[get_coord(col, row)] == white_pawn)
            {
                pawn_max[col][white] = row;
                break;
            }
        for(auto row = max_row - pawn_default_row;
            row >= pawn_default_row;
            row--)
            if(b[get_coord(col, row)] == black_pawn)
            {
                pawn_min[col][0] = max_row - row;
                break;
            }
        for(auto row = pawn_default_row;
            row <= max_row - pawn_default_row;
            row++)
            if(b[get_coord(col, row)] == black_pawn)
            {
                pawn_max[col][0] = max_row - row;
                break;
            }
    }
}




//-----------------------------
k2chess::eval_t k2eval::CountKingTropism(const bool king_color)
{
    auto occ_cr = 0;
    auto rit = coords[!king_color].rbegin();
    ++rit;
    for(; rit != coords[!king_color].rend(); ++rit)
    {
        auto type = get_type(b[*rit]);
        if(type == pawn)
            break;
        auto dist = king_dist(*king_coord[king_color], *rit);
        if(dist >= 4)
            continue;
        occ_cr += tropism_factor[dist < 3][type];
    }
    return occ_cr;
}





//-----------------------------
void k2eval::MoveKingTropism(const coord_t from_coord, const move_c move,
                             const bool king_color)
{
    state[ply].tropism[black] = king_tropism[black];
    state[ply].tropism[white] = king_tropism[white];

    auto type = get_type(b[move.to_coord]);

    if(type == king)
    {
        king_tropism[king_color] = CountKingTropism(king_color);
        king_tropism[!king_color] = CountKingTropism(!king_color);

        return;
    }
    auto k_coord = *king_coord[king_color];
    auto dist_to = king_dist(k_coord, move.to_coord);
    auto dist_fr = king_dist(k_coord, from_coord);


    if(dist_fr < 4 && !(move.flag & is_promotion))
        king_tropism[king_color] -= tropism_factor[dist_fr < 3][type];
    if(dist_to < 4)
        king_tropism[king_color] += tropism_factor[dist_to < 3][type];

    auto cap_t = get_type(k2chess::state[ply].captured_piece);
    if(move.flag & is_capture)
    {
        dist_to = king_dist(*king_coord[!king_color], move.to_coord);
        if(dist_to < 4)
            king_tropism[!king_color] -= tropism_factor[dist_to < 3][cap_t];
    }

#ifndef NDEBUG
    auto tmp = CountKingTropism(king_color);
    if(king_tropism[king_color] != tmp && cap_t != king)
        ply = ply;
    tmp = CountKingTropism(!king_color);
    if(king_tropism[!king_color] != tmp && cap_t != king)
        ply = ply;
#endif // NDEBUG
}





//-----------------------------
bool k2eval::MakeMove(const move_c m)
{
    state[ply].val_opn = val_opn;
    state[ply].val_end = val_end;

    bool is_special_move = k2chess::MakeMove(m);

    auto from_coord = k2chess::state[ply].from_coord;

    MoveKingTropism(from_coord, m, wtm);

    MovePawnStruct(b[m.to_coord], from_coord, m);

    return is_special_move;
}





//-----------------------------
void k2eval::TakebackMove(const move_c m)
{
    king_tropism[black] = state[ply].tropism[black];
    king_tropism[white] = state[ply].tropism[white];

    auto from_coord = k2chess::state[ply].from_coord;

    k2chess::TakebackMove(m);

    ply++;
    wtm ^= white;
    MovePawnStruct(b[from_coord], from_coord, m);
    wtm ^= white;
    ply--;

    val_opn = state[ply].val_opn;
    val_end = state[ply].val_end;
}





//-----------------------------
k2chess::eval_t k2eval::KingOpenFiles(const bool king_color)
{
    auto ans = 0;
    auto k = *king_coord[king_color];

    if(get_col(k) == 0)
        k++;
    else if(get_col(k) == max_col)
        k--;

    int open_files_near_king = 0, open_files[3] = {0};
    for(auto i = 0; i < 3; ++i)
    {
        auto col = get_col(k) + i - 1;
        if(col < 0 || col > max_col)
            continue;
        if(pawn_max[col][!king_color] == 0)
        {
            open_files_near_king++;
            open_files[i]++;
        }
    }

    if(open_files_near_king == 0)
        return 0;

    int rooks_queens_on_file[board_width] = {0};
    auto rit = coords[!king_color].rbegin();
    ++rit;
    for(; rit != coords[!king_color].rend(); ++rit)
    {
        auto type = get_type(b[*rit]);
        if(type != queen && type != rook)
            break;
        auto k = pawn_max[get_col(*rit)][king_color] ? 1 : 2;
        rooks_queens_on_file[get_col(*rit)] +=
            k*(type == rook ? 2 : 1);
    }

    for(auto i = 0; i < 3; ++i)
    {
        if(open_files[i])
            ans += rooks_queens_on_file[get_col(k) + i - 1];
    }
    if(ans <= 2)
        ans = ans/2;

    return ans;
}





//-----------------------------
k2chess::eval_t k2eval::KingWeakness(const bool king_color)
{
    auto ans = 0;
    auto k = *king_coord[king_color];
    auto shft = king_color ? board_width : -board_width;

    if(get_col(k) == 0)
        k++;
    else if(get_col(k) == max_col)
        k--;

    if(get_col(k) == 2 || get_col(k) == 5)
        ans += 30;
    if(get_col(k) == 3 || get_col(k) == 4)
    {
        // if able to castle
        if(k2chess::state[ply].castling_rights & (0x0C >> 2*king_color))
            ans += 30;
        else
            ans += 60;
    }
    if((king_color == white && get_row(k) > 1)
            || (king_color == black && get_row(k) < 6))
    {
        ans += 60;
    }


    auto idx = 0;
    for(auto col = 0; col < 3; ++col)
    {
        if(col + k + 2*shft - 1 < 0
                || col + k + 2*shft - 1 >= (eval_t)(sizeof(b)/sizeof(*b)))
            continue;
        auto pt1 = b[col + k + shft - 1];
        auto pt2 = b[col + k + 2*shft - 1];
        const piece_t pwn = black_pawn | king_color;
        if(pt1 == pwn || pt2 == pwn)
            continue;

        if(is_light(pt1, king_color) && is_light(pt2, king_color))
            continue;

        if(col + k + shft - 2 < 0)
            continue;

        if(is_light(pt2, king_color)
                && (b[col + k + shft - 2] == pwn
                    || b[col + k + shft + 0] == pwn))
            continue;

        idx += (1 << col);
    }
    idx = 7 - idx;
    // cases: ___, __p, _p_, _pp, p__, p_p, pp_, ppp
    size_t cases[] = {0, 1, 1, 3, 1, 2, 3, 4};
    eval_t scores[] = {140, 75, 75, 10, 0};

    ans += scores[cases[idx]];

    if(cases[idx] != 4)
        ans += 25*KingOpenFiles(king_color);

    return ans;
}





//-----------------------------
void k2eval::KingSafety(const bool king_color)
{
    eval_t trp = king_tropism[king_color];

    if(quantity[!king_color][queen] == 0)
    {
        trp += trp*trp/15;
        val_opn += king_color ? -trp : trp;

        return;
    }

    if(trp == 21 || trp == 10)
        trp /= 4;
    else if(trp >= 60)
        trp *= 4;
    trp += trp*trp/5;

    if(material[!king_color]/100 - pieces[!king_color] < 24)
        trp /= 2;

    auto kw = KingWeakness(king_color);
    if(trp <= 6)
        kw /= 2;
    else if(kw >= 40)
        trp *= 2;

    if(trp > 500)
        trp = 500;
    // if able to castle
    if(k2chess::state[ply].castling_rights & (0x0C >> 2*king_color))
        kw /= 4;
    else
        kw = (material[!king_color]/100 + 24)*kw/72;

    auto ans = -3*(kw + trp)/2;

    val_opn += king_color ? ans : -ans;
}





//-----------------------------
k2eval::k2eval() : material_values_opn {0, 0, queen_val_opn, rook_val_opn,
            bishop_val_opn, kinght_val_opn, pawn_val_opn},
    material_values_end {0, 0, queen_val_end, rook_val_end,
                           bishop_val_end, kinght_val_end, pawn_val_end},
    tropism_factor  //  k  Q   R   B   N   P
{
    {0, 0, 10, 10, 10,  4, 4},  // 4 >= dist > 3
    {0, 0, 21, 21, 10,  0, 10}  // dist < 3
},
pst
{
    {
        // KING
        {   {-8,   -18,   -18,   -28,   -28,   -18,   -18,    -8},
            {-8,   -18,   -18,   -28,   -28,   -18,   -18,    -8},
            {-8,   -18,   -18,   -28,   -28,   -18,   -18,    -8},
            {-8,   -18,   -18,   -28,   -28,   -18,   -18,    -8},
            { 2,    -8,    -8,   -18,   -18,    -8,    -8,     2},
            {12,     2,     2,     2,     2,     2,     2,    12},
            {42,    42,    22,    22,    22,    22,    42,    42},
            {42,    62,    32,    22,    22,    32,    62,    42}
        },

        {   {-60,   -45,   -31,   -16,   -16,   -31,   -45,   -60},
            {-40,   -15,    -1,    14,    14,    -1,   -15,   -40},
            {-30,     0,    34,    40,    40,    34,     0,   -30},
            {-30,     0,    40,    50,    50,    40,     0,   -30},
            {-30,     0,    40,    50,    50,    40,     0,   -30},
            {-30,     0,    34,    40,    40,    34,     0,   -30},
            {-40,   -15,    -1,    14,    14,    -1,   -15,   -40},
            {-60,   -45,   -31,   -16,   -16,   -31,   -45,   -60}
        }
    },

    {
        // QUEEN
        {   {-16,    -6,    -6,    -1,    -1,    -6,    -6,   -16},
            { -6,     4,     4,     4,     4,     4,     4,    -6},
            { -6,     4,     4,     4,     4,     4,     4,    -6},
            { -1,     4,     4,     4,     4,     4,     4,    -1},
            { -1,     4,     4,     4,     4,     4,     4,    -1},
            { -6,     4,     4,     4,     4,     4,     4,    -6},
            { -6,     4,     4,     4,     4,     4,     4,    -6},
            {-16,    -6,    -6,    -1,    -1,    -6,    -6,   -16}
        },

        {   {-17,    -7,    -7,    -2,    -2,    -7,    -7,   -17},
            { -7,     3,     3,     3,     3,     3,     3,    -7},
            { -7,     3,     8,     8,     8,     8,     3,    -7},
            { -2,     3,     8,     8,     8,     8,     3,    -2},
            { -2,     3,     8,     8,     8,     8,     3,    -2},
            { -7,     3,     8,     8,     8,     8,     3,    -7},
            { -7,     3,     3,     3,     3,     3,     3,    -7},
            {-17,    -7,    -7,    -7,    -7,    -7,    -7,   -17},
        }
    },

    {
        // ROOK
        {   {  0,     0,     0,     0,     0,     0,     0,     0},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            {  0,     0,     0,     0,     0,     0,     0,     0}
        },

        {   {  0,     0,     0,     0,     0,     0,     0,     0},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            { -5,     0,     0,     0,     0,     0,     0,    -5},
            {  0,     0,     0,     0,     0,     0,     0,     0}
        }
    },

    {
        //BISHOP
        {   {-20,   -10,   -10,   -10,   -10,   -10,   -10,   -20},
            {-10,     0,     0,     0,     0,     0,     0,   -10},
            {-10,     0,     5,    10,    10,     5,     0,   -10},
            {-10,     5,     5,    10,    10,     5,     5,   -10},
            {-10,     0,    10,    10,    10,    10,     0,   -10},
            {-10,    10,    10,    10,    10,    10,    10,   -10},
            {-10,     5,     0,     0,     0,     0,     5,   -10},
            {-20,   -10,   -10,   -10,   -10,   -10,   -10,   -20}
        },

        {   {-18,    -8,    -8,    -8,    -8,    -8,    -8,   -18},
            { -8,     2,     2,     2,     2,     2,     2,    -8},
            { -8,     2,     7,    12,    12,     7,     2,    -8},
            { -8,     7,     7,    12,    12,     7,     7,    -8},
            { -8,     2,    12,    12,    12,    12,     2,    -8},
            { -8,    12,    12,    12,    12,    12,    12,    -8},
            { -8,     7,     2,     2,     2,     2,     7,    -8},
            {-18,    -8,    -8,    -8,    -8,    -8,    -8,   -18}
        }
    },

    {
        //KNIGHT
        {   {-38,   -28,   -18,   -18,   -18,   -18,   -28,   -38},
            {-28,    -8,    12,    12,    12,    12,    -8,   -28},
            {  0,    25,    30,    40,    40,    30,    25,     0},
            {-10,    17,    42,    50,    50,    42,    17,   -10},
            {-18,    12,    27,    32,    32,    27,    12,   -18},
            {-18,    17,    22,    27,    27,    22,    17,   -18},
            {-28,    -8,    12,    17,    17,    12,    -8,   -28},
            {-38,   -28,   -18,   -18,   -18,   -18,   -28,   -38}
        },

        {   {-75,   -25,   -15,   -15,   -15,   -15,   -25,   -75},
            {-25,    -5,    15,    15,    15,    15,    -5,   -25},
            {-15,    15,    25,    30,    30,    25,    15,   -15},
            {-15,    20,    30,    35,    35,    30,    20,   -15},
            {-15,    15,    30,    35,    35,    30,    15,   -15},
            {-15,    20,    25,    30,    30,    25,    20,   -15},
            {-25,    -5,    15,    20,    20,    15,    -5,   -25},
            {-75,   -25,   -15,   -15,   -15,   -15,   -25,   -75}
        }
    },

    {
        //PAWN
        {   {-11,   -11,   -11,   -11,   -11,   -11,   -11,   -11},
            { 39,    39,    39,    39,    39,    39,    39,    39},
            { -1,    -1,     9,    19,    19,     9,    -1,    -1},
            { -6,    -6,    -1,    14,    14,    -1,    -6,    -6},
            {-11,   -11,   -11,     9,     9,   -11,   -11,   -11},
            { -6,   -16,   -21,   -11,   -11,   -21,   -16,    -6},
            { -6,    -1,    -1,   -31,   -31,    -1,    -1,    -6},
            {-11,   -11,   -11,   -11,   -11,   -11,   -11,   -11},
        },

        {   { 0,      0,     0,     0,     0,     0,     0,     0},
            { 0,      0,     0,     0,     0,     0,     0,     0},
            { 0,      0,     0,     0,     0,     0,     0,     0},
            { 0,      0,     0,     0,     0,     0,     0,     0},
            { 0,      0,     0,     0,     0,     0,     0,     0},
            { 0,      0,     0,     0,     0,     0,     0,     0},
            { 0,      0,     0,     0,     0,     0,     0,     0},
            { 0,      0,     0,     0,     0,     0,     0,     0},
        }
    }
}

{
    pawn_max = &p_max[1];
    pawn_min = &p_min[1];
    state = &e_state[prev_states];

    val_opn = 0;
    val_end = 0;

    state[ply].val_opn = 0;
    state[ply].val_end = 0;

    p_max[0][black] = 0;
    p_max[0][white] = 0;
    p_min[0][black] = 7;
    p_min[0][white] = 7;
    p_max[9][black] = 0;
    p_max[9][white] = 0;
    p_min[9][black] = 7;
    p_min[9][white] = 7;

    king_tropism[white] = 0;
    king_tropism[black] = 0;
}




#ifndef NDEBUG
//-----------------------------
void k2eval::RunUnitTests()
{
	k2chess::RunUnitTests();

	assert(king_dist(get_coord("e5"), get_coord("f6")) == 1);
	assert(king_dist(get_coord("e5"), get_coord("d3")) == 2);
	assert(king_dist(get_coord("h1"), get_coord("h8")) == 7);
	assert(king_dist(get_coord("h1"), get_coord("a8")) == 7);

	SetupPosition("5k2/8/2P5/8/8/8/8/K7 w - - 0 1");
	assert(IsUnstoppablePawn(2, white, white));
	assert(!IsUnstoppablePawn(2, white, black));
	SetupPosition("k7/7p/8/8/8/8/8/1K6 b - - 0 1");
	assert(IsUnstoppablePawn(7, black, black));
	assert(!IsUnstoppablePawn(7, black, white));
	SetupPosition("8/7p/8/8/7k/8/8/K7 w - - 0 1");
	assert(IsUnstoppablePawn(7, black, black));
	assert(!IsUnstoppablePawn(7, black, white));

    SetupPosition("3k4/p7/B1Pp4/7p/K3P3/7P/2n5/8 w - - 0 1");
    assert(IsPasser(0, black));
    assert(IsPasser(2, white));
    assert(!IsPasser(3, black));
    assert(!IsPasser(4, white));
    assert(!IsPasser(7, black));
    assert(!IsPasser(7, white));
    assert(!IsPasser(1, black));

    SetupPosition("4k3/1p6/8/8/1P6/8/1P6/4K3 w - - 0 1");
    val_opn = 0;
    val_end = 0;
    EvalPawns(white);
    assert(val_opn == pawn_dbl_iso_opn);
    assert(val_end == pawn_dbl_iso_end);
    val_opn = 0;
    val_end = 0;
    EvalPawns(black);
    assert(val_opn == -pawn_iso_opn);
    assert(val_end == -pawn_iso_end);
    SetupPosition("4k3/1pp5/1p6/8/1P6/1P6/1P6/4K3 w - - 0 1");
    val_opn = 0;
    val_end = 0;
    EvalPawns(white);
    assert(val_opn == pawn_dbl_iso_opn);
    assert(val_end == pawn_dbl_iso_end);
    val_opn = 0;
    val_end = 0;
    EvalPawns(black);
    assert(val_opn == -pawn_dbl_opn);
    assert(val_end == -pawn_dbl_end);
    SetupPosition("4k3/2p1p1p1/2Np1pPp/7P/1p6/Pr1PnP2/1P2P3/4K3 b - - 0 1");
    val_opn = 0;
    val_end = 0;
    EvalPawns(white);
    assert(val_opn == 2*pawn_hole_opn + pawn_gap_opn);
    assert(val_end == 2*pawn_hole_end + pawn_gap_end);
    val_opn = 0;
    val_end = 0;
    EvalPawns(black);
    assert(val_opn == -pawn_hole_opn - pawn_gap_opn);
    assert(val_end == -pawn_hole_end - pawn_gap_end);
    SetupPosition("8/8/3K1P2/3p2P1/1Pkn4/8/8/8 w - - 0 1");
    val_opn = 0;
    val_end = 0;
    EvalPawns(white);
    assert(val_opn == pawn_pass_3/pawn_pass_opn_divider +
           pawn_pass_4/pawn_pass_opn_divider +
           pawn_pass_5/pawn_pass_opn_divider + pawn_iso_opn);
    assert(val_end == pawn_pass_3 + pawn_pass_4 + pawn_pass_5 +
           2*pawn_king_tropism3 - pawn_king_tropism1 - 3*pawn_king_tropism2 +
           5*pawn_pass_connected + pawn_iso_end);
    val_opn = 0;
    val_end = 0;
    EvalPawns(black);
    assert(val_opn == -pawn_blk_pass_3/pawn_pass_opn_divider - pawn_iso_opn);
    assert(val_end == -pawn_blk_pass_3 - pawn_king_tropism1 -
           3*pawn_king_tropism2 + pawn_king_tropism3 - pawn_iso_end);
}
#endif // NDEBUG
