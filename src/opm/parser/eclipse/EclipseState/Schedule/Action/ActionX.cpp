/*
  Copyright 2018 Statoil ASA.

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

#include <sstream>
#include <unordered_set>

#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Action/ActionX.hpp>

#include "ActionValue.hpp"
#include "ActionParser.hpp"

namespace Opm {
namespace Action {


bool ActionX::valid_keyword(const std::string& keyword) {
    static std::unordered_set<std::string> actionx_whitelist = {"WELSPECS","WELOPEN"};
    return (actionx_whitelist.find(keyword) != actionx_whitelist.end());
}


ActionX::ActionX(const std::string& name, size_t max_run, double min_wait, std::time_t start_time) :
    m_name(name),
    m_max_run(max_run),
    m_min_wait(min_wait),
    m_start_time(start_time)
{}


ActionX::ActionX(const DeckRecord& record, std::time_t start_time) :
    ActionX( record.getItem("NAME").getTrimmedString(0),
             record.getItem("NUM").get<int>(0),
             record.getItem("MIN_WAIT").getSIDouble(0),
             start_time )

{}


ActionX::ActionX(const DeckKeyword& kw, std::time_t start_time) :
    ActionX(kw.getRecord(0), start_time)
{
    std::vector<std::string> tokens;
    for (size_t record_index = 1; record_index < kw.size(); record_index++) {
        const auto& record = kw.getRecord(record_index);
        const auto& cond_tokens = record.getItem("CONDITION").getData<std::string>();

        for (const auto& token : cond_tokens)
            tokens.push_back(token);

        this->m_conditions.emplace_back(cond_tokens, kw.location());
    }
    this->condition = Action::AST(tokens);
}


void ActionX::addKeyword(const DeckKeyword& kw) {
    this->keywords.push_back(kw);
}



Action::Result ActionX::eval(std::time_t sim_time, const Action::Context& context) const {
    if (!this->ready(sim_time))
        return Action::Result(false);

    auto result = this->condition.eval(context);

    if (result) {
        this->run_count += 1;
        this->last_run = sim_time;
    }

    return result;
}


bool ActionX::ready(std::time_t sim_time) const {
  if (this->run_count >= this->max_run())
        return false;

    if (sim_time < this->start_time())
        return false;

    if (this->run_count == 0)
        return true;

    if (this->min_wait() <= 0)
        return true;

    return std::difftime(sim_time, this->last_run) > this->min_wait();
}


std::vector<DeckKeyword>::const_iterator ActionX::begin() const {
    return this->keywords.begin();
}

std::vector<DeckKeyword>::const_iterator ActionX::end() const {
    return this->keywords.end();
}


std::vector<std::string> ActionX::keyword_strings() const {
    std::vector<std::string> keyword_strings;
    std::string keyword_string;
    {
        std::stringstream ss;

        for (const auto& kw : this->keywords)
            ss << kw;

        keyword_string = ss.str();
    }

    std::size_t offset = 0;
    while (true) {
        auto eol_pos = keyword_string.find('\n', offset);
        if (eol_pos == std::string::npos)
            break;

        if (eol_pos > offset)
            keyword_strings.push_back(keyword_string.substr(offset, eol_pos - offset));

        offset = eol_pos + 1;
    }
    keyword_strings.push_back("ENDACTIO");

    return keyword_strings;
}


const std::vector<Condition>& ActionX::conditions() const {
    return this->m_conditions;
}

}
}
