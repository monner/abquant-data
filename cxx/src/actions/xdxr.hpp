/****************************************************************************
 * Copyright (c) 2020-2016 Jimmy M. Gong                                    *
 * All rights reserved.                                                     *
 *                                                                          *
 * Distributed under the terms of the Apache License 2.0 License.           *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#pragma once

#include <memory>
#include <type_traits>

#include "DataFrame/DataFrame.h"
#include "DataFrame/DataFrameOperators.h"
// #include "DataFrame/DataFrameVisitors.h"
#include <DataFrame/Vectors/HeteroVector.h>

#include "DataFrame/Utils/DateTime.h"
#include "abquant/actions/stockxdxr.hpp"
#include "abquant/actions/utils.hpp"
#include "xtensor/xadapt.hpp"
#include "xtensor/xarray.hpp"
#include "xtensor/xio.hpp"
#include "xtensor/xmanipulation.hpp"
#include "xtensor/xview.hpp"

namespace abq
{
using namespace std;

template <class SA>
class Xdxr
{
public:
    Xdxr(SA& sa)
    {
        m_codes                              = sa.getCodes();
        std::shared_ptr<StockXdxrAction> sap = std::make_shared<StockXdxrAction>(m_codes, 1);
        m_xdxr_df                            = sap->toDataFrame();
    };

    ~Xdxr() = default;
    MyDataFrame getXdxr(MyDataFrame& df, FQ_TYPE fq);

private:
    void fillConcatDataframe(MyDataFrame& df) const;
    /**
     *  \brief concat, minic pandas' concat
     *
     *  Detailed description
     *
     *  \param MyDataFrame ldf: left side df, bqf
     *  \param MyDataFrame rdf: right side df, stockxdxr info
     *  \return return MyDataFrame
     */
    MyDataFrame concat(MyDataFrame& ldf, MyDataFrame& rdf) const;
    MyDataFrame calc(MyDataFrame& df, FQ_TYPE fq) const;
    QStringList m_codes;
    MyDataFrame m_xdxr_df;
};

template <class SA>
MyDataFrame Xdxr<SA>::getXdxr(MyDataFrame& df, FQ_TYPE fq)
{
    MyDataFrame rdf = concat(df, m_xdxr_df);
    fillConcatDataframe(rdf);
    return calc(rdf, fq);
}

template <class SA>
MyDataFrame Xdxr<SA>::concat(MyDataFrame& ldf, MyDataFrame& rdf) const
{
    MyDataFrame join_df =
        ldf.join_by_index<std::decay_t<decltype(rdf)>, std::string, double, int>(rdf, join_policy::left_right_join);
    // join_df.write<std::ostream, std::string, double, int>(std::cout);
    return join_df;
}

template <class SA>
void Xdxr<SA>::fillConcatDataframe(MyDataFrame& df) const
{
    df.fill_missing<double, 1>({"if_trade"}, fill_policy::value, {0});
    df.fill_missing<double, 6>(
        {
            "open",
            "close",
            "high",
            "low",
            "if_trade",
            "category",
        },
        fill_policy::fill_forward);
    df.fill_missing<double, 4>(
        {
            "fenhong",
            "peigu",
            "peigujia",
            "songzhuangu",
        },
        fill_policy::value, {0});
    df.fill_missing<double, 1>({"if_trade"}, fill_policy::value, {1});
}

template <class SA>
MyDataFrame Xdxr<SA>::calc(MyDataFrame& df, FQ_TYPE fq) const
{
    auto lhs_code      = df.get_column<string>("lhs.code");
    auto rhs_code      = df.get_column<string>("rhs.code");
    auto open        = df.get_column<double>("open");
    auto close       = df.get_column<double>("close");
    auto high        = df.get_column<double>("high");
    auto low         = df.get_column<double>("low");
    auto vol         = df.get_column<double>("vol");
    auto fenhong     = df.get_column<double>("fenhong");
    auto peigu       = df.get_column<double>("peigu");
    auto peigujia    = df.get_column<double>("peigujia");
    auto songzhuangu = df.get_column<double>("songzhuangu");

    xt::xarray<double> xopen        = xt::adapt(open);
    xt::xarray<double> xclose       = xt::adapt(close);
    xt::xarray<double> xhigh        = xt::adapt(high);
    xt::xarray<double> xlow         = xt::adapt(low);
    xt::xarray<double> xvol         = xt::adapt(vol);
    xt::xarray<double> xfenhong     = xt::adapt(fenhong);
    xt::xarray<double> xpeigu       = xt::adapt(peigu);
    xt::xarray<double> xpeigujia    = xt::adapt(peigujia);
    xt::xarray<double> xsongzhuangu = xt::adapt(songzhuangu);

    auto xclose_sf1              = xt::roll(xclose, 1);
    *xclose_sf1.begin()          = std::numeric_limits<double>::quiet_NaN();
    xt::xarray<double> xpreclose = (xclose_sf1 * 10 - xfenhong + xpeigu * xpeigujia) / (10 + xpeigu + xsongzhuangu);

    xt::xarray<double> adj = {};
    if (fq == FQ_TYPE::PRE) {
        // todo: may be need to fill the head or tail with a pre/post value, instead of NAN
        adj           = xt::roll(xpreclose, -1);
        *adj.rbegin() = std::numeric_limits<double>::quiet_NaN();
        adj           = xt::eval(adj / xclose);
        // fillna(1)
        xfillna<double>(adj, 1);
        // minic pandas cumprod, flip first
        adj = xt::flip(adj, 0);
        adj = xt::nancumprod(adj);
        // minic pandas cumprod, need flip again
        adj = xt::flip(adj, 0);
    } else {
        // todo: may be need to fill the head or tail with a pre/post value, instead of NAN
        adj           = xt::roll(xpreclose, -1);
        *adj.rbegin() = std::numeric_limits<double>::quiet_NaN();
        adj           = xt::eval(xclose / adj);
        adj           = xt::nancumprod(adj);
        // shift(1)
        adj          = xt::roll(adj, 1);
        *adj.begin() = std::numeric_limits<double>::quiet_NaN();
        // fillna(1)
        xfillna<double>(adj, 1);
    }
    xopen     = xopen * adj;
    xclose    = xclose * adj;
    xhigh     = xhigh * adj;
    xlow      = xlow * adj;
    xpreclose = xpreclose * adj;
    xvol      = xvol / adj;

    df.load_column<double>("open", {xopen.begin(), xopen.end()}, nan_policy::pad_with_nans);
    df.load_column<double>("high", {xhigh.begin(), xhigh.end()}, nan_policy::pad_with_nans);
    df.load_column<double>("close", {xclose.begin(), xclose.end()}, nan_policy::pad_with_nans);
    df.load_column<double>("low", {xlow.begin(), xlow.end()}, nan_policy::pad_with_nans);
    df.load_column<double>("preclose", {xpreclose.begin(), xpreclose.end()}, nan_policy::pad_with_nans);
    df.load_column<double>("vol", {xvol.begin(), xvol.end()}, nan_policy::pad_with_nans);
    df.load_column<double>("adj", {adj.begin(), adj.end()}, nan_policy::pad_with_nans);

    df.remove_column("fenhong");
    df.remove_column("peigu");
    df.remove_column("peigujia");
    df.remove_column("songzhuangu");
    df.remove_column("suogu");
    df.remove_column("liquidity_before");
    df.remove_column("liquidity_after");

    auto functor = [](const std::string&, const int& if_trade, const double& open) -> bool {
        return (if_trade == 1 && open != 0);
    };
    auto res = df.get_data_by_sel<double, double, decltype(functor), double, std::string>("if_trade", "open", functor);
    return res;
}

} // namespace abq
