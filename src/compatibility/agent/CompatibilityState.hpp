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
/**
 * @file
 * State and CLI options for Compatibility agent
 */

#ifndef COMPATIBILITY_AGENT_STATE_HPP
#define COMPATIBILITY_AGENT_STATE_HPP

#include "libfossdbmanagerclass.hpp"
#include "CompatibilityAgent.hpp"

using namespace std;

/**
 * @class CompatibilityCliOptions
 * @brief Store the options sent through the CLI
 */
//class CompatibilityAgent;
class CompatibilityCliOptions
{
  private:
    int verbosity;  /**< The verbosity level */
    bool json;      /**< Whether to generate JSON output */

  public:
    bool isVerbosityDebug() const;
    bool doJsonOutput() const;

    CompatibilityCliOptions(int verbosity, bool json);
    CompatibilityCliOptions();
};

/**
 * @class CompatibilityState
 * @brief Store the state of the agent
 */
class CompatibilityState
{
  public:
    CompatibilityState(const int agentId, const CompatibilityCliOptions &cliOptions);

    void setAgentId(const int agentId);
    int getAgentId() const;
    const CompatibilityCliOptions& getCliOptions() const;
    const CompatibilityAgent& getCompatibilityAgent() const;

  private:
    int agentId;                      						/**< Agent id */
    const CompatibilityCliOptions cliOptions;   			/**< CLI options passed */
    const CompatibilityAgent compatibilityAgent;            /**< Compatibility agent object */
};

#endif // COMPATIBILITY_AGENT_STATE_HPP
