<?php
/***********************************************************
 * Copyright (C) 2019, Siemens AG
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
 ***********************************************************/

namespace Fossology\Compatibility\Ui;

use Fossology\Lib\Plugin\AgentPlugin;

class CompatibilityAgentPlugin extends AgentPlugin
{
  public function __construct() {
    $this->Name = "agent_compatibility";
    $this->Title =  _("Compatibility License Analysis, scanning for licenses compatibility");
    $this->AgentName = "compatibility";

    parent::__construct();
  }

  function AgentHasResults($uploadId=0)
  {
    return CheckARS($uploadId, $this->AgentName, "compatibility agent", "compatibility_ars");
  }
  
 public function AgentAdd($jobId, $uploadId, &$errorMsg, $dependencies=array(), $arguments=null)
  {
    $compatibilityDependencies = array("agent_adj2nest");

    $compatibilityDependencies = array_merge($compatibilityDependencies,
      $this->getCompatibilityDependencies($_POST));

    return $this->doAgentAdd($jobId, $uploadId, $errorMsg,
      array_unique($compatibilityDependencies), $arguments);
  }
  
  private function getCompatibilityDependencies($request)
  {
    $dependencies = array();
    if (array_key_exists("Check_agent_nomos", $request) && $request["Check_agent_nomos"]==1) {
      $dependencies[] = "agent_nomos";
    }
    if (array_key_exists("Check_agent_monk", $request) && $request["Check_agent_monk"]==1) {
      $dependencies[] = "agent_monk";
    }
    if (array_key_exists("Check_agent_ojo", $request) && $request["Check_agent_ojo"]==1) {
      $dependencies[] = "agent_ojo";
    }
    if (array_key_exists("Check_agent_ninka", $request) && $request["Check_agent_ninka"]==1) {
      $dependencies[] = "agent_ninka";
    }
    
    return $dependencies;
  }
  
}
register_plugin(new CompatibilityAgentPlugin());
