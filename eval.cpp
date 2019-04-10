#include "eval.h"





//-----------------------------
k2chess::eval_t k2eval::Eval()
{
    state[ply].val_opn = val_opn;
    state[ply].val_end = val_end;

    for(auto color: {black, white})
    {
        EvalPawns(color);
        EvalRooks(color);
        EvalKingSafety(color);
        EvalMobility(color);
    }
    EvalImbalances();

    auto ans = -ReturnEval(wtm);
    ans -= side_to_move_bonus;

    val_opn = state[ply].val_opn;
    val_end = state[ply].val_end;

    return ans;
}





//-----------------------------
void k2eval::FastEval(const move_c move)
{
    eval_t ansO = 0, ansE = 0;

    auto x = get_col(move.to_coord);
    auto y = get_row(move.to_coord);
    auto x0 = get_col(move.from_coord);
    auto y0 = get_row(move.from_coord);
    auto to_type = get_type(b[move.to_coord]);
    if(!wtm)
    {
        y = max_row - y;
        y0 = max_row - y0;
    }

    coord_t type;
    auto flag = move.flag & is_promotion;
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

    if(move.flag & is_capture)
    {
        auto captured_piece = k2chess::state[ply].captured_piece;
        if(move.flag & is_en_passant)
        {
            type = pawn;
            ansO += material_values_opn[type] +
                    pst[type - 1][opening][max_row - y0][x];
            ansE += material_values_end[type] +
                    pst[type - 1][endgame][max_row - y0][x];
        }
        else
        {
            type = get_type(captured_piece);
            ansO += material_values_opn[type] +
                    pst[type - 1][opening][max_row - y][x];
            ansE += material_values_end[type] +
                    pst[type - 1][endgame][max_row - y][x];
        }
    }
    else if(move.flag & is_right_castle)
    {
        type = rook;
        auto rook_col = default_king_col + cstl_move_length - 1;
        ansO += pst[type - 1][opening][max_row][rook_col] -
                pst[type - 1][opening][max_row][max_col];
        ansE += pst[type - 1][endgame][max_row][rook_col] -
                pst[type - 1][endgame][max_row][max_col];
    }
    else if(move.flag & is_left_castle)
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
                row_ = max_row - row;

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
    bool opp_only_pawns = material[!stm]/centipawn == pieces[!stm] - 1;

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
        auto k_coord = king_coord(stm);
        auto opp_k_coord = king_coord(!stm);
        auto pawn_coord = get_coord(col, stm ? mx + 1 : max_row - mx - 1);
        auto k_dist = king_dist(k_coord, pawn_coord);
        auto opp_k_dist = king_dist(opp_k_coord, pawn_coord);

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
    int dist = king_dist(king_coord(!side_of_pawn), promo_square);
    auto k_coord = king_coord(side_of_pawn);
    if(get_col(k_coord) == col && king_dist(k_coord, promo_square) <= max_row - pmax)
        pmax--;
    return dist - (side_of_pawn  != stm) > max_row - pmax;
}





//-----------------------------
void k2eval::EvalMobility(bool stm)
{
    eval_t f_type[] = {0, 0, mob_queen, mob_rook, mob_bishop, mob_knight};
    eval_t f_num[] = {-15, -15, -15, -10, -5, 5, 10, 15, 18, 20,
                      21, 21, 21, 22, 22};

    eval_t ans = 0;
    auto mask = exist_mask[stm] & slider_mask[stm];
    while(mask)
    {
        const auto piece_id = lower_bit_num(mask);
        mask ^= (1 << piece_id);
        const auto coord = coords[stm][piece_id];
        const auto type = get_type(b[coord]);
        int cr = sum_directions[stm][piece_id];
        if(type == queen)
            cr /= 2;
        assert(cr < 15);
        ans += f_type[type]*f_num[cr];
    }
    ans /= mobility_divider;

    val_opn += stm ? ans : -ans;
    val_end += stm ? ans : -ans;
}





//-----------------------------
void k2eval::EvalImbalances()
{
    const auto X = material[black]/centipawn + 1 + material[white]/centipawn
            + 1 - pieces[black] - pieces[white];

    if(X == 3 && (material[black]/centipawn == 4 ||
                  material[white]/centipawn == 4))
    {
        // KNk, KBk, Kkn, Kkb
        if(pieces[black] + pieces[white] == 3)
        {
            val_opn = 0;
            val_end = 0;
            return;
        }
        // KPkn, KPkb
        if(material[white]/centipawn == 1 && material[black]/centipawn == 4)
            val_end += bishop_val_end + pawn_val_end/4;
        // KNkp, KBkp
        if(material[black]/centipawn == 1 && material[white]/centipawn == 4)
            val_end -= bishop_val_end + pawn_val_end/4;
    }
    // KNNk, KBNk, KBBk, etc
    else if(X == 6 && (material[black] == 0 || material[white] == 0))
    {
        if(quantity[white][pawn] != 0 || quantity[black][pawn] != 0)
            return;
        if(quantity[white][knight] == 2
                || quantity[black][knight] == 2)
        {
            val_opn = 0;
            val_end = 0;
            return;
        }
        // many code for mating with only bishop and knight
        else if((quantity[white][knight] == 1 &&
                 quantity[white][bishop] == 1) ||
                (quantity[black][knight] == 1 &&
                 quantity[black][bishop] == 1))
        {
            auto stm = black;
            if(quantity[white][knight] == 1)
                stm = white;
            const auto bishop_mask = exist_mask[stm] & type_mask[stm][bishop];
            const auto bishop_id = lower_bit_num(bishop_mask);
            eval_t ans = 0;
            const auto o_col = get_col(king_coord(!stm));
            const auto o_row = get_row(king_coord(!stm));
            if((o_col == max_col - 1 && (o_row <= 1 || o_row >= max_row - 1)) ||
                    (o_col == max_col && (o_row == 0 || o_row == max_row)))
                ans = imbalance_king_in_corner;
            if((o_col == 0 && (o_row <= 1 || o_row >= max_row - 1)) ||
                    (o_col == 1 && (o_row == 0 || o_row == max_row)))
                ans = -imbalance_king_in_corner;
            const auto coord = coords[stm][bishop_id];
            bool bishop_on_light_sq = ((get_col(coord)) + get_row(coord)) & 1;
            if(!bishop_on_light_sq)
                ans = -ans;
            if(!stm)
                ans = -ans;
            val_end += ans;
        }
    }
    else if(X == 3)
    {
        if(quantity[black][pawn] + quantity[white][pawn] != 1)
            return;
        // KBPk with pawn at first(last) file and bishop with 'good' color
        const bool stm = material[white] == 0;
        const auto pawn_col_min = pawn_max[0][!stm];
        const auto pawn_col_max = pawn_max[max_col][!stm];
        if(pawn_col_max == 0 && pawn_col_min == 0)
            return;
        const auto bishop_mask = exist_mask[!stm] & type_mask[!stm][bishop];
        if(!bishop_mask)
            return;
        const auto coord = coords[!stm][lower_bit_num(bishop_mask)];
        const auto sq = get_coord(pawn_col_max == 0 ? 0 : max_col,
                                  stm ? 0 : max_row);
        if(is_same_color(coord, sq))
            return;

        if(king_dist(king_coord(stm), sq) <= 1)
        {
            val_opn = 0;
            val_end = 0;
            return;
        }
    }
    else if(X == 0 && (material[white] + material[black])/centipawn == 1)
    {
        // KPk
        const bool stm = material[white]/centipawn == 1;
        const auto pawn_mask = exist_mask[stm] & type_mask[stm][pawn];
        const auto pawn_id = lower_bit_num(pawn_mask);
        const auto coord = coords[stm][pawn_id];
        const auto colp = get_col(coord);
        const bool unstop = IsUnstoppablePawn(colp, stm, wtm);
        const auto k_coord = king_coord(stm);
        const auto opp_k_coord = king_coord(!stm);
        const auto dist_k = king_dist(k_coord, coord);
        const auto dist_opp_k = king_dist(opp_k_coord, coord);

        if(!unstop && dist_k > dist_opp_k + (wtm == stm))
        {
            val_opn = 0;
            val_end = 0;
            return;
        }
        else if((colp == 0 || colp == max_col))
        {
            const auto sq = get_coord(colp, stm ? max_row : 0);
            if(king_dist(opp_k_coord, sq) <= 1)
            {
                val_opn = 0;
                val_end = 0;
                return;
            }
        }
    }

    // two bishops
    if(quantity[white][bishop] == 2)
    {
        val_opn += bishop_pair;
        val_end += bishop_pair;
    }
    if(quantity[black][bishop] == 2)
    {
        val_opn -= bishop_pair;
        val_end -= bishop_pair;
    }

    // pawn absence for both sides
    if(quantity[white][pawn] == 0
            && quantity[black][pawn] == 0
            && material[white] != 0 && material[black] != 0)
        val_end /= imbalance_no_pawns;

    // multicolored bishops
    if(quantity[white][bishop] == 1
            && quantity[black][bishop] == 1)
    {
        const auto wb_mask = exist_mask[white] & type_mask[white][bishop];
        const auto wb_id = lower_bit_num(wb_mask);
        const auto bb_mask = exist_mask[black] & type_mask[black][bishop];
        const auto bb_id = lower_bit_num(bb_mask);
        if(!is_same_color(coords[white][wb_id], coords[black][bb_id]))
        {
            if(material[white]/centipawn - pieces[white] == 4 - 2
                    && material[black]/centipawn - pieces[black] == 4 - 2)
                val_end /= imbalance_multicolor1;
            else
                val_end = val_end*imbalance_multicolor2/imbalance_multicolor3;

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

    EvalKingSafety(white);
    std::cout << "King safety white\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);
    EvalKingSafety(black);
    std::cout << "King safety black\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);

    EvalMobility(white);
    std::cout << "Mobility white\t\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);
    EvalMobility(black);
    std::cout << "Mobility black\t\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);

    EvalRooks(white);
    std::cout << "Rooks white\t\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);
    EvalRooks(black);
    std::cout << "Rooks black\t\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);

    EvalImbalances();
    std::cout << "Imbalances summary\t";
    std::cout << val_opn - store_vo << '\t' << val_end - store_ve << '\t'
              << ReturnEval(white) - store_sum << std::endl;
    store_vo = val_opn;
    store_ve = val_end;
    store_sum = ReturnEval(white);

    auto ans = -ReturnEval(wtm);
    ans -= side_to_move_bonus;
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
void k2eval::MovePawnStruct(const piece_t moved_piece, const move_c move)
{
    if(get_type(moved_piece) == pawn || (move.flag & is_promotion))
    {
        SetPawnStruct(get_col(move.to_coord));
        if(move.flag)
            SetPawnStruct(get_col(move.from_coord));
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
bool k2eval::MakeMove(const move_c move)
{
    state[ply].val_opn = val_opn;
    state[ply].val_end = val_end;

    bool is_special_move = k2chess::MakeMove(move);

    MovePawnStruct(b[move.to_coord], move);

    return is_special_move;
}





//-----------------------------
void k2eval::TakebackMove(const move_c move)
{
    k2chess::TakebackMove(move);

    ply++;
    wtm ^= white;
    MovePawnStruct(b[move.from_coord], move);
    wtm ^= white;
    ply--;

    val_opn = state[ply].val_opn;
    val_end = state[ply].val_end;
}





//-----------------------------
void k2eval::EvalRooks(const bool stm)
{
    auto ans = 0;
    auto rooks_on_last_cr = 0;
    auto mask = exist_mask[stm] & type_mask[stm][rook];
    while(mask)
    {
        auto rook_id = lower_bit_num(mask);
        mask ^= (1 << rook_id);
        const auto coord = coords[stm][rook_id];
        if((stm && get_row(coord) >= max_row - 1) ||
                (!stm && get_row(coord) <= 1))
            rooks_on_last_cr++;
        if(quantity[stm][pawn] >= rook_max_pawns_for_open_file &&
                pawn_max[get_col(coord)][stm] == 0)
            ans += (pawn_max[get_col(coord)][!stm] == 0 ? rook_open_file :
                                                         rook_semi_open_file);
    }
    ans += rooks_on_last_cr*rook_on_last_rank;
    val_opn += (stm ? ans : -ans);
}





//-----------------------------
bool k2eval::Sheltered(const coord_t k_col, coord_t k_row,
                       const bool stm) const
{
    if(!col_within(k_col))
        return false;
    if((stm && k_row > 1) || (!stm && k_row < max_row - 1))
        return false;
    const auto shft = stm ? board_width : -board_width;
    const auto coord = get_coord(k_col, k_row);
    const auto p = black_pawn ^ stm;
    if(coord + 2*shft < 0 || coord + 2*shft >= board_size)
        return false;
    if(b[coord + shft] == p)
        return true;
    else
    {
        const auto pc1 = b[coord + shft];
        const auto pc2 = b[coord + 2*shft];
        const auto pc1_r = col_within(k_col + 1) ?
                    b[coord + shft + 1] : empty_square;
        const auto pc1_l = col_within(k_col - 1) ?
                    b[coord + shft - 1] : empty_square;
        if(pc2 == p && (pc1_r == p || pc1_l == p))
            return true;
        if(pc1 != empty_square && get_color(pc1) == stm &&
                pc2 != empty_square && get_color(pc2) == stm)
            return true;

    }
    return false;
}





//-----------------------------
bool k2eval::KingHasNoShelter(coord_t k_col, coord_t k_row,
                              const bool stm) const
{
    if(k_col == 0)
        k_col++;
    else if(k_col == max_col)
        k_col--;
    const i32 cstl[] = {black_can_castle_left | black_can_castle_right,
                        white_can_castle_left | white_can_castle_right};
    if(quantity[!stm][queen] == 0 ||
            k2chess::state[ply].castling_rights & cstl[stm])
        return false;
    if(!Sheltered(k_col, k_row, stm))
    {
        if(k_col > 0 && k_col < max_col &&
                (stm ? k_row < max_row : k_row > 0) &&
                b[get_coord(k_col, k_row+(stm ? 1 : -1))] != empty_square &&
                Sheltered(k_col - 1, k_row, stm) &&
                Sheltered(k_col + 1, k_row, stm))
            return false;
        else
            return true;
    }
    if(!Sheltered(k_col - 1, k_row, stm) &&
            !Sheltered(k_col + 1, k_row, stm))
        return true;
    return 0;
}





//-----------------------------
k2chess::attack_t k2eval::KingSafetyBatteries(const coord_t targ_coord,
                                              const attack_t att,
                                              const bool stm) const
{
    auto msk = att & slider_mask[!stm];
    auto ans = att;
    while(msk)
    {
        const auto piece_id = lower_bit_num(msk);
        msk ^= (1 << piece_id);
        const auto coord1 = coords[!stm][piece_id];
        auto maybe = attacks[!stm][coord1] & slider_mask[!stm];
        if(!maybe)
            continue;
        while(maybe)
        {
            const auto j = lower_bit_num(maybe);
            maybe ^= (1 << j);
            const auto coord2 = coords[!stm][j];
            const auto type1 = get_type(b[coord1]);
            const auto type2 = get_type(b[coord2]);
            bool is_ok = type1 == type2 || type2 == queen;
            if(!is_ok && type1 == queen)
            {
                const auto col1 = get_col(coord1);
                const auto col2 = get_col(coord2);
                const auto col_t = get_col(targ_coord);
                const auto row1 = get_row(coord1);
                const auto row2 = get_row(coord2);
                const auto row_t = get_row(targ_coord);
                bool like_rook = (col1 == col2 && col1 == col_t) ||
                        (row1 == row2 && row1 == row_t);
                bool like_bishop = sgn(col1 - col_t) == sgn(col2 - col1) &&
                        sgn(row1 - row_t) == sgn(row2 - row_t);
                if(type2 == rook && like_rook)
                    is_ok = true;
                else if(type2 == bishop && like_bishop)
                    is_ok = true;
            }
            if(is_ok && IsOnRay(coord1, targ_coord, coord2) &&
                    IsSliderAttack(coord1, coord2))
                ans |= (1 << j);
        }
    }
    return ans;
}





//-----------------------------
size_t k2eval::CountAttacksOnKing(const bool stm, const coord_t k_col,
                                  const coord_t k_row)
{
    size_t ans = 0;
    for(auto delta_col = -1; delta_col <= 1; ++delta_col)
        for(auto delta_row = -1; delta_row <= 1; ++delta_row)
        {
            const auto col = k_col + delta_col;
            const auto row = k_row + delta_row;
            if(!row_within(row) || !col_within(col))
                continue;
            const auto targ_coord = get_coord(col, row);
            const auto att = attacks[!stm][targ_coord];
            if(!att)
                continue;
            const auto all_att = KingSafetyBatteries(targ_coord, att, stm);
            ans += __builtin_popcount(all_att);
        }
    return ans;
}





//-----------------------------
void k2eval::EvalKingSafety(const bool stm)
{
    if(quantity[!stm][queen] < 1 && quantity[!stm][rook] < 2)
        return;
    const auto k_col = get_col(king_coord(stm));
    const auto k_row = get_row(king_coord(stm));

    const auto attacks = CountAttacksOnKing(stm, k_col, k_row);
    const bool no_shelter = KingHasNoShelter(k_col, k_row, stm);
    const auto f_saf = no_shelter ? king_saf_attack1*king_saf_attack2/10 :
                                    king_saf_attack2;
    const auto f_queen = quantity[!stm][queen] == 0 ? king_saf_no_queen : 10;
    const auto center = (std::abs(2*k_col - max_col) <= 1 &&
            quantity[!stm][queen] != 0) ? king_saf_central_files : 0;

    const auto ans = center + king_saf_no_shelter*no_shelter +
            attacks*attacks*f_saf/f_queen;

    val_opn += stm ? -ans : ans;
}





//-----------------------------
k2eval::k2eval() : material_values_opn {0, 0, queen_val_opn, rook_val_opn,
            bishop_val_opn, kinght_val_opn, pawn_val_opn},
    material_values_end {0, 0, queen_val_end, rook_val_end,
                           bishop_val_end, kinght_val_end, pawn_val_end},
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

    InitPawnStruct();
    InitEvalOfMaterialAndPst();
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

    SetupPosition("5k2/8/2P5/8/8/8/8/K7 w - -");
    assert(IsUnstoppablePawn(2, white, white));
    assert(!IsUnstoppablePawn(2, white, black));
    SetupPosition("k7/7p/8/8/8/8/8/1K6 b - -");
    assert(IsUnstoppablePawn(7, black, black));
    assert(!IsUnstoppablePawn(7, black, white));
    SetupPosition("8/7p/8/8/7k/8/8/K7 w - -");
    assert(IsUnstoppablePawn(7, black, black));
    assert(!IsUnstoppablePawn(7, black, white));

    SetupPosition("3k4/p7/B1Pp4/7p/K3P3/7P/2n5/8 w - -");
    assert(IsPasser(0, black));
    assert(IsPasser(2, white));
    assert(!IsPasser(3, black));
    assert(!IsPasser(4, white));
    assert(!IsPasser(7, black));
    assert(!IsPasser(7, white));
    assert(!IsPasser(1, black));

    SetupPosition("4k3/1p6/8/8/1P6/8/1P6/4K3 w - -");
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
    SetupPosition("4k3/1pp5/1p6/8/1P6/1P6/1P6/4K3 w - -");
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
    SetupPosition("4k3/2p1p1p1/2Np1pPp/7P/1p6/Pr1PnP2/1P2P3/4K3 b - -");
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
    SetupPosition("8/8/3K1P2/3p2P1/1Pkn4/8/8/8 w - -");
    val_opn = 0;
    val_end = 0;
    EvalPawns(white);
    assert(val_opn == pawn_pass_3/pawn_pass_opn_divider +
           pawn_pass_4/pawn_pass_opn_divider +
           pawn_pass_5/pawn_pass_opn_divider + pawn_iso_opn);
    assert(val_end == pawn_pass_3 + pawn_pass_4 + pawn_pass_5 +
           2*pawn_king_tropism3 - pawn_king_tropism1 -
           3*pawn_king_tropism2 +
           5*pawn_pass_connected + pawn_iso_end);
    val_opn = 0;
    val_end = 0;
    EvalPawns(black);
    assert(val_opn == -pawn_blk_pass_3/pawn_pass_opn_divider - pawn_iso_opn);
    assert(val_end == -pawn_blk_pass_3 - pawn_king_tropism1 -
           3*pawn_king_tropism2 + pawn_king_tropism3 - pawn_iso_end);

    // endings
    SetupPosition("5k2/8/8/N7/8/8/8/4K3 w - -");  // KNk
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("5k2/8/8/n7/8/8/8/4K3 w - -");  // Kkn
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("5k2/8/8/B7/8/8/8/4K3 w - -");  // KBk
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("5k2/8/8/b7/8/8/8/4K3 w - -");  // Kkb
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("5k2/p7/8/b7/8/8/P7/4K3 w - -");  // KPkbp
    EvalImbalances();
    assert(val_end < bishop_val_end);
    SetupPosition("5k2/8/8/n7/8/8/P7/4K3 w - -");  // KPkn
    EvalImbalances();
    assert(val_end > pawn_val_end/4);
    SetupPosition("5k2/p7/8/B7/8/8/8/4K3 w - -");  // KBkp
    EvalImbalances();
    assert(val_end < -pawn_val_end/4);
    SetupPosition("5k2/p6p/8/B7/8/8/7P/4K3 w - -");  // KBPkpp
    EvalImbalances();
    assert(val_end > 2*pawn_val_end);
    SetupPosition("5k2/8/8/NN6/8/8/8/4K3 w - -");  // KNNk
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("5k2/8/8/nn6/8/8/8/4K3 w - -");  // Kknn
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("5k2/p7/8/nn6/8/8/P7/4K3 w - -");  // KPknnp
    EvalImbalances();
    assert(val_end < -2*kinght_val_end + pawn_val_end);
    SetupPosition("5k2/8/8/nb6/8/8/8/4K3 w - -");  // Kknb
    EvalImbalances();
    assert(val_end < -kinght_val_end - bishop_val_end + pawn_val_end);
    SetupPosition("5k2/8/8/1b6/8/7p/8/7K w - -");  // Kkbp not drawn
    EvalImbalances();
    assert(val_end < -bishop_val_end - pawn_val_end/2);
    SetupPosition("7k/8/7P/1B6/8/8/8/6K1 w - -");  // KBPk drawn
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("5k2/8/8/3b4/p7/8/K7/8 w - -");  // Kkbp drawn
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("7k/8/P7/1B6/8/8/8/6K1 w - -");  // KBPk not drawn
    EvalImbalances();
    assert(val_end > bishop_val_end + pawn_val_end/2);
    SetupPosition("2k5/8/P7/1B6/8/8/8/6K1 w - -");  // KBPk not drawn
    EvalImbalances();
    assert(val_end > bishop_val_end + pawn_val_end/2);
    SetupPosition("1K6/8/P7/1B6/8/8/8/6k1 w - -");  // KBPk not drawn
    EvalImbalances();
    assert(val_end > bishop_val_end + pawn_val_end/2);
    SetupPosition("1k6/8/1P6/1B6/8/8/8/6K1 w - -");  // KBPk not drawn
    EvalImbalances();
    assert(val_end > bishop_val_end + pawn_val_end/2);
    SetupPosition("1k6/8/8/P7/8/4N3/8/6K1 w - -");  // KNPk
    EvalImbalances();
    assert(val_end > kinght_val_end + pawn_val_end/2);
    SetupPosition("8/8/8/5K2/2k5/8/2P5/8 b - -");  // KPk
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("8/8/8/5K2/2k5/8/2P5/8 w - -");  // KPk
    EvalImbalances();
    assert(val_end > pawn_val_end/2);
    SetupPosition("k7/2K5/8/P7/8/8/8/8 w - -");  // KPk
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("8/8/8/p7/8/1k6/8/1K6 w - -");  // Kkp
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);
    SetupPosition("8/8/8/p7/8/1k6/8/2K5 w - -");  // Kkp
    EvalImbalances();
    assert(val_end < -pawn_val_end/2);
    SetupPosition("8/8/8/pk6/8/K7/8/8 w - -");  // Kkp
    EvalImbalances();
    assert(val_end < -pawn_val_end/2);
    SetupPosition("8/8/8/1k5p/8/8/8/K7 w - -");  // Kkp
    EvalImbalances();
    assert(val_end < -pawn_val_end/2);

    // bishop pairs
    SetupPosition("rn1qkbnr/pppppppp/8/8/8/8/PPPPPPPP/R1BQKBNR w KQkq -");
    val_opn = 0;
    val_end = 0;
    EvalImbalances();
    assert(val_opn == bishop_pair && val_end == bishop_pair);
    SetupPosition("1nb1kb2/4p3/8/8/8/8/4P3/1NB1K1N1 w - -");
    val_opn = 0;
    val_end = 0;
    EvalImbalances();
    assert(val_opn == -bishop_pair && val_end == -bishop_pair);
    SetupPosition("1nb1k1b1/4p3/8/8/8/8/4P3/1NB1K1N1 w - -");
    val_opn = 0;
    val_end = 0;
    EvalImbalances();
    assert(val_opn == -bishop_pair && val_end == -bishop_pair);
    SetupPosition("1nb1kbb1/4p3/8/8/8/8/4P3/1NB1K1N1 w - -");
    val_opn = 0;
    val_end = 0;
    EvalImbalances();
    assert(val_opn == 0 && val_end == 0);  // NB

    SetupPosition("4kr2/8/8/8/8/8/8/1BBNK3 w - -"); // pawn absense
    EvalImbalances();
    assert(val_end < bishop_val_end/2);

    // multicolored bishops
    SetupPosition("rn1qkbnr/p1pppppp/8/8/8/8/PPPPPPPP/RN1QKBNR w KQkq -");
    auto tmp = val_end;
    EvalImbalances();
    assert(val_end < pawn_val_end && val_end < tmp);
    SetupPosition("rnbqk1nr/p1pppppp/8/8/8/8/PPPPPPPP/RNBQK1NR w KQkq -");
    tmp = val_end;
    EvalImbalances();
    assert(val_end < pawn_val_end && val_end < tmp);
    SetupPosition("rnbqk1nr/p1pppppp/8/8/8/8/PPPPPPPP/RN1QKBNR w KQkq -");
    tmp = val_end;
    EvalImbalances();
    assert(val_end == tmp);
    SetupPosition("rn1qkbnr/p1pppppp/8/8/8/8/PPPPPPPP/RNBQK1NR w KQkq -");
    tmp = val_end;
    EvalImbalances();
    assert(val_end == tmp);
    SetupPosition("4kb2/pppp4/8/8/8/8/PPPPPPPP/4KB2 w - -");
    EvalImbalances();
    assert(val_end < 5*pawn_val_end/2);

    //king safety
    SetupPosition("4k3/4p3/8/8/8/4P3/5P2/4K3 w - -");
    assert(Sheltered(4, 0, white));
    assert(Sheltered(4, 7, black));
    SetupPosition("4k3/4r3/4p3/8/8/4B3/4R3/4K3 w - -");
    assert(Sheltered(4, 0, white));
    assert(Sheltered(4, 7, black));
    SetupPosition("4k3/8/4b3/8/8/4P3/3N4/4K3 w - -");
    assert(!Sheltered(4, 0, white));
    assert(!Sheltered(4, 7, black));
    SetupPosition("4k3/4q3/8/8/8/8/8/4K3 w - -");
    assert(!Sheltered(4, 0, white));
    assert(!Sheltered(4, 7, black));

    SetupPosition("q5k1/pp3p1p/6p1/8/8/8/PP3PPP/Q5K1 w - -");
    assert(KingHasNoShelter(6, 0, white) == false);
    assert(KingHasNoShelter(6, 7, black) == false);
    SetupPosition("q5k1/pp4pp/5p2/8/8/7P/PP3PP1/Q5K1 w - -");
    assert(KingHasNoShelter(6, 0, white) == false);
    assert(KingHasNoShelter(6, 7, black) == false);
    SetupPosition("q5k1/pp3pbp/6n1/8/8/6P1/PP3P1P/Q5K1 w - -");
    assert(KingHasNoShelter(6, 0, white) == false);
    assert(KingHasNoShelter(6, 7, black) == false);
    SetupPosition("q5k1/pp3nbr/5nnq/8/8/5P1N/PP4PB/Q5K1 w - -");
    assert(KingHasNoShelter(6, 0, white) == false);
    assert(KingHasNoShelter(6, 7, black) == false);
    SetupPosition("q5k1/pp4bn/6nr/8/8/5P2/PP4P1/Q5K1 w - -");
    assert(KingHasNoShelter(6, 0, white) == false);
    assert(KingHasNoShelter(6, 7, black) == false);
    SetupPosition("4k2r/pp6/8/8/8/8/PP6/Q5K1 w k -");
    assert(KingHasNoShelter(6, 0, white) == false);
    assert(KingHasNoShelter(4, 7, black) == false);
    SetupPosition("q3kr2/pp1p1p2/8/8/5P1P/8/PP4P1/Q5K1 w - -");
    assert(KingHasNoShelter(6, 0, white) == true);
    assert(KingHasNoShelter(4, 7, black) == true);
    SetupPosition("q5k1/pp3p1p/7p/8/8/5P2/PP3P1P/Q5K1 w - -");
    assert(KingHasNoShelter(6, 0, white) == true);
    assert(KingHasNoShelter(4, 7, black) == true);
    SetupPosition("q5k1/pp4p1/6p1/8/8/6N1/PP3P1P/Q5K1 w - -");
    assert(KingHasNoShelter(6, 0, white) == true);
    assert(KingHasNoShelter(4, 7, black) == true);

    SetupPosition("6k1/pp1N1ppp/6r1/8/4n2q/B3p3/PP4P1/Q4RK1 w - -");
    assert(CountAttacksOnKing(white, 6, 0) == 3 + 1 + 1 + 0 + 1);
    assert(CountAttacksOnKing(black, 6, 7) == 1 + 0 + 0 + 2 + 0);
    SetupPosition("2q2rk1/pp3pp1/2r4p/2r5/8/2P4Q/PPP2PPR/1K5R w - -");
    assert(CountAttacksOnKing(white, 1, 0) == 0);
    assert(CountAttacksOnKing(black, 6, 7) == 0);
    SetupPosition("2q2r2/pp3pk1/2r3p1/2r5/8/7Q/PPP2PPR/1K5R w - -");
    assert(CountAttacksOnKing(white, 1, 0) == 0 + 0 + 2 + 0 + 0);
    assert(CountAttacksOnKing(black, 6, 6) == 0+0+2 + 0+0+2 + 0+0+2);
}
#endif // NDEBUG
