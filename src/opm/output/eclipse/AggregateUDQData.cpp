/*
  Copyright 2018 Statoil ASA

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <opm/output/eclipse/AggregateUDQData.hpp>
#include <opm/output/eclipse/AggregateGroupData.hpp>
#include <opm/output/eclipse/WriteRestartHelpers.hpp>

#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Runspec.hpp>
//#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>


#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQConfig.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQActive.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQDefine.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQAssign.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQParams.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQFunctionTable.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <iostream>

// #####################################################################
// Class Opm::RestartIO::Helpers::AggregateGroupData
// ---------------------------------------------------------------------


namespace {

    // maximum number of groups
    std::size_t ngmaxz(const std::vector<int>& inteHead)
    {
        return inteHead[20];
    }


    namespace iUdq {

        Opm::RestartIO::Helpers::WindowedArray<int>
        allocate(const std::vector<int>& udqDims)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<int>;
            return WV {
                WV::NumWindows{ static_cast<std::size_t>(udqDims[0]) },
                WV::WindowSize{ static_cast<std::size_t>(udqDims[1]) }
            };
        }

        template <class IUDQArray>
        void staticContrib(const Opm::UDQInput& udq_input, IUDQArray& iUdq)
        {
            if (udq_input.is<Opm::UDQDefine>()) {
                iUdq[0] = 2;
                iUdq[1] = -4;
            } else {
                iUdq[0] = 0;
                iUdq[1] = -4;
            }
            iUdq[2] = udq_input.index.typed_insert_index;
        }

    } // iUdq

    namespace iUad {

        Opm::RestartIO::Helpers::WindowedArray<int>
        allocate(const std::vector<int>& udqDims)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<int>;
            return WV {
                WV::NumWindows{ static_cast<std::size_t>(udqDims[2]) },
                    WV::WindowSize{ static_cast<std::size_t>(udqDims[3]) }
            };
        }

        template <class IUADArray>
        void staticContrib(const Opm::UDQActive::Record& udq_record, IUADArray& iUad)
        {
            iUad[0] = udq_record.uad_code;
            iUad[1] = udq_record.input_index + 1;

            // entry 3  - unknown meaning - value = 1
            iUad[2] = 1;

            iUad[3] = udq_record.use_count;
            iUad[4] = udq_record.use_index + 1;
        }
    } // iUad


    namespace zUdn {

        Opm::RestartIO::Helpers::WindowedArray<
            Opm::EclIO::PaddedOutputString<8>
        >
        allocate(const std::vector<int>& udqDims)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<
                Opm::EclIO::PaddedOutputString<8>
            >;

            return WV {
                WV::NumWindows{ static_cast<std::size_t>(udqDims[0]) },
                WV::WindowSize{ static_cast<std::size_t>(udqDims[4]) }
            };
        }

    template <class zUdnArray>
    void staticContrib(const Opm::UDQInput& udq_input, zUdnArray& zUdn)
    {
        // entry 1 is udq keyword
        zUdn[0] = udq_input.keyword();
        zUdn[1] = udq_input.unit();
    }
    } // zUdn

    namespace zUdl {

        Opm::RestartIO::Helpers::WindowedArray<
            Opm::EclIO::PaddedOutputString<8>
        >
        allocate(const std::vector<int>& udqDims)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<
                Opm::EclIO::PaddedOutputString<8>
            >;

            return WV {
                WV::NumWindows{ static_cast<std::size_t>(udqDims[0]) },
                WV::WindowSize{ static_cast<std::size_t>(udqDims[5]) }
            };
        }

    template <class zUdlArray>
    void staticContrib(const Opm::UDQInput& input, zUdlArray& zUdl)
        {
            int l_sstr = 8;
            int max_l_str = 128;
            // write out the input formula if key is a DEFINE udq
            if (input.is<Opm::UDQDefine>()) {
                const auto& udq_define = input.get<Opm::UDQDefine>();
                const std::string& z_data = udq_define.input_string();
                int n_sstr =  z_data.size()/l_sstr;
                if (static_cast<int>(z_data.size()) > max_l_str) {
                    std::cout << "Too long input data string (max 128 characters): " << z_data << std::endl;
                    throw std::invalid_argument("UDQ - variable: " + udq_define.keyword());
                }
                else {
                    for (int i = 0; i < n_sstr; i++) {
                        zUdl[i] = z_data.substr(i*l_sstr, l_sstr);
                    }
                    //add remainder of last non-zero string
                    if ((z_data.size() % l_sstr) > 0)
                        zUdl[n_sstr] = z_data.substr(n_sstr*l_sstr);
                }
            }
        }
    } // zUdl

    namespace iGph {

        Opm::RestartIO::Helpers::WindowedArray<int>
        allocate(const std::vector<int>& udqDims)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<int>;
            return WV {
                WV::NumWindows{ static_cast<std::size_t>(udqDims[6]) },
                WV::WindowSize{ static_cast<std::size_t>(1) }
            };
        }

        template <class IGPHArray>
        void staticContrib(const int    inj_phase,
                           IGPHArray&   iGph)
        {
                iGph[0] = inj_phase;
        }
    } // iGph

    namespace iUap {

        Opm::RestartIO::Helpers::WindowedArray<int>
        allocate(const std::vector<int>& udqDims)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<int>;
            return WV {
                WV::NumWindows{ static_cast<std::size_t>(udqDims[7]) },
                WV::WindowSize{ static_cast<std::size_t>(1) }
            };
        }

        template <class IUAPArray>
        void staticContrib(const int    wg_no,
                           IUAPArray&   iUap)
        {
                iUap[0] = wg_no+1;
        }
    } // iUap

}

// =====================================================================


template < typename T>
std::pair<bool, int > findInVector(const std::vector<T>  & vecOfElements, const T  & element)
{
    std::pair<bool, int > result;

    // Find given element in vector
    auto it = std::find(vecOfElements.begin(), vecOfElements.end(), element);

    if (it != vecOfElements.end())
    {
        result.second = std::distance(vecOfElements.begin(), it);
        result.first = true;
    }
    else
    {
        result.first = false;
        result.second = -1;
    }
    return result;
}

const std::vector<int> Opm::RestartIO::Helpers::igphData::ig_phase(const Opm::Schedule& sched,
                                                                   const std::size_t simStep,
                                                                   const std::vector<int>& inteHead
                                                                  )
{
    std::vector<int> inj_phase(ngmaxz(inteHead), 0);

    for (const auto& gname : sched.groupNames(simStep)) {
        const auto& group = sched.getGroup2(gname, simStep);
        if (group.isInjectionGroup()) {
            //auto phase = group.getInjectionPhase();
            auto phase = Opm::Phase::OIL;
            if ( phase == Opm::Phase::OIL   ) inj_phase[group.insert_index()] = 1;
            if ( phase == Opm::Phase::WATER ) inj_phase[group.insert_index()] = 2;
            if ( phase == Opm::Phase::GAS   ) inj_phase[group.insert_index()] = 3;
        }
        
    }
    return inj_phase;
}

const std::vector<int> iuap_data(const Opm::Schedule& sched,
                                    const std::size_t simStep,
                                    const std::vector<Opm::UDQActive::InputRecord>& iuap)
{
    //construct the current list of well or group sequence numbers to output the IUAP array
    std::vector<int> wg_no;
    Opm::UDAKeyword wg_key;
    
    for (std::size_t ind = 0; ind < iuap.size(); ind++) {
        auto& ctrl = iuap[ind].control;
        wg_key = Opm::UDQ::keyword(ctrl);
        if ((wg_key == Opm::UDAKeyword::WCONPROD) || (wg_key == Opm::UDAKeyword::WCONINJE)) {
            const auto& well = sched.getWell2(iuap[ind].wgname, simStep);
            wg_no.push_back(well.seqIndex());
        }
        else if ((wg_key == Opm::UDAKeyword::GCONPROD) || (wg_key == Opm::UDAKeyword::GCONINJE)) {
            const auto& group = sched.getGroup2(iuap[ind].wgname, simStep);
            wg_no.push_back(group.insert_index());
        }
        else {
            std::cout << "Invalid Control keyword: " << static_cast<int>(ctrl) << std::endl;
            throw std::invalid_argument("UDQ - variable: " + iuap[ind].udq );
        }
            
    }

    return wg_no;
}



Opm::RestartIO::Helpers::AggregateUDQData::
AggregateUDQData(const std::vector<int>& udqDims)
    : iUDQ_ (iUdq::allocate(udqDims)),
      iUAD_ (iUad::allocate(udqDims)),
      zUDN_ (zUdn::allocate(udqDims)),
      zUDL_ (zUdl::allocate(udqDims)),
      iGPH_ (iGph::allocate(udqDims)),
      iUAP_ (iUap::allocate(udqDims))
{}

// ---------------------------------------------------------------------

void
Opm::RestartIO::Helpers::AggregateUDQData::
captureDeclaredUDQData(const Opm::Schedule&                 sched,
                       const std::size_t                    simStep,
                       const std::vector<int>&              inteHead)
{
    auto udqCfg = sched.getUDQConfig(simStep);
    for (const auto& udq_input : udqCfg.input()) {
        auto udq_index = udq_input.index.insert_index;
        {
            auto i_udq = this->iUDQ_[udq_index];
            iUdq::staticContrib(udq_input, i_udq);
        }
        {
            auto z_udn = this->zUDN_[udq_index];
            zUdn::staticContrib(udq_input, z_udn);
        }
        {
            auto z_udl = this->zUDL_[udq_index];
            zUdl::staticContrib(udq_input, z_udl);
        }
    }

    auto udq_active = sched.udqActive(simStep);
    if (udq_active) {
        const auto& udq_records = udq_active.get_iuad();
        for (std::size_t index = 0; index < udq_records.size(); index++) {
            const auto& record = udq_records[index];
            auto i_uad = this->iUAD_[index];
            iUad::staticContrib(record, i_uad);
        }
        
        const auto& iuap_records = udq_active.get_iuap();
        const auto iuap_vect = iuap_data(sched, simStep,iuap_records);
        for (std::size_t index = 0; index < iuap_vect.size(); index++) {
            const auto& wg_no = iuap_vect[index];
            auto i_uap = this->iUAP_[index];
            iUap::staticContrib(wg_no, i_uap);
        }

    }
    Opm::RestartIO::Helpers::igphData igph_dat;
    auto igph = igph_dat.ig_phase(sched, simStep, inteHead);
    for (std::size_t index = 0; index < igph.size(); index++) {
            auto i_igph = this->iGPH_[index];
            iGph::staticContrib(igph[index], i_igph);
        }
        
}
