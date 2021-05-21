/*
 * Copyright (C) 2020, Siemens AG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "CompatibilityState.hpp"

/**
 * Constructor for State
 * @param agentId    Agent ID
 * @param cliOptions CLI options passed
 */
CompatibilityState::CompatibilityState(const int agentId, const CompatibilityCliOptions &cliOptions) :
  agentId(agentId), cliOptions(cliOptions)
{
}

/**
 * Get the agent id
 * @return Agent id
 */
void CompatibilityState::setAgentId(const int agentId)
{
  this->agentId = agentId;
}

/**
 * Get the agent id
 * @return Agent id
 */
int CompatibilityState::getAgentId() const
{
  return agentId;
}

/**
 * Get the CompatibilityAgent reference
 * @return CompatibilityAgent reference
 */
const CompatibilityAgent& CompatibilityState::getCompatibilityAgent() const
{
  return compatibilityAgent;
}

/**
 * @brief Constructor for CompatibilityCliOptions
 * @param verbosity Verbosity set by CLI
 * @param json      True to get output in JSON format
 */
CompatibilityCliOptions::CompatibilityCliOptions(int verbosity, bool json) :
    verbosity(verbosity), json(json)
{
}

/**
 * @brief Default constructor for CompatibilityCliOptions
 */
CompatibilityCliOptions::CompatibilityCliOptions() :
    verbosity(0), json(false)
{
}

/**
 * @brief Get the CompatibilityCliOptions set by user
 * @return The CompatibilityCliOptions
 */
const CompatibilityCliOptions& CompatibilityState::getCliOptions() const
{
  return cliOptions;
}

/**
 * @brief Check if verbosity is set
 * @return True if set, else false
 */
bool CompatibilityCliOptions::isVerbosityDebug() const
{
  return verbosity >= 1;
}

/**
 * @brief Check if JSON output is required
 * @return True if required, else false
 */
bool CompatibilityCliOptions::doJsonOutput() const
{
  return json;
}
