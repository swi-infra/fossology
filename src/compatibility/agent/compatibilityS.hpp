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

#ifndef COMPATIBILITY_AGENT_COMPATIBILITY_HPP
#define COMPATIBILITY_AGENT_COMPATIBILITY_HPP

#include <iostream>
#include "yaml-cpp/yaml.h"
#include "json/json.h"
#include <fstream>
#include <tuple>
#include <sstream>

#include "CompatibilityAgent.hpp"
#include "CompatibilityUtils.hpp"

extern "C" {
#include "libfossagent.h"
}

using namespace std;
vector<tuple<string,string,string>> compatibilityFunc(vector<string> myVec, string fileName, string lic_types, string rule);

#endif // COMPATIBILITY_AGENT_OJOS_HPP
