/*
  Copyright 2013 Statoil ASA.

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

#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleEnums.hpp>
#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <iostream>

namespace Opm {
    
    EclipseState::EclipseState(DeckConstPtr deck) {
        initPhases(deck);
        initSchedule(deck);
        initTitle(deck);
    }
    

    ScheduleConstPtr EclipseState::getSchedule() const {
        return schedule;
    }


    std::string EclipseState::getTitle() const {
        return m_title;
    }

    void EclipseState::initSchedule(DeckConstPtr deck) {
        schedule = ScheduleConstPtr( new Schedule(deck) );
    }


    void EclipseState::initPhases(DeckConstPtr deck) {
        if (deck->hasKeyword("OIL"))
            phases.insert(PhaseEnum::OIL);

        if (deck->hasKeyword("GAS"))
            phases.insert(PhaseEnum::GAS);

        if (deck->hasKeyword("WATER"))
            phases.insert(PhaseEnum::WATER);
    }


    bool EclipseState::hasPhase(enum PhaseEnum phase) const {
         return (phases.count(phase) == 1);
    }

    void EclipseState::initTitle(DeckConstPtr deck){
        DeckKeywordConstPtr titleKeyword = deck->getKeyword("TITLE");
        DeckRecordConstPtr record = titleKeyword->getRecord(0);
        DeckItemPtr item = record->getItem(0);
        std::vector<std::string> itemValue = item->getStringData();
        m_title.clear();
        for(size_t i=0; i!=itemValue.size(); ++i) {
            m_title.append(itemValue[i]);
            if (i<itemValue.size()-1) m_title.append(" ");
        }
    }
}
