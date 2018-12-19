<?php
/*
 Copyright (C) 2015 Siemens AG

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/**
 * @dir
 * @brief UI for SPDX2 agent
 */
namespace Fossology\SpdxTwo;

use Fossology\Lib\Plugin\AgentPlugin;

/**
 * @class SpdxTwoAgentPlugin
 * @brief Generate SPDX2 report for multiple uploads
 */
class SpdxTwoAgentPlugin extends AgentPlugin
{
  public function __construct() {
    $this->Name = "agent_spdx2";
    $this->Title =  _("SPDX2 generation");
    $this->AgentName = "spdx2";

    parent::__construct();
  }

  /**
   * @brief Add uploads to report
   * @param array $uploads Array of upload ids
   * @return string
   */
  public function uploadsAdd($uploads)
  {
    if (count($uploads) == 0) {
      return '';
    }
    return '--uploadsAdd='. implode(',', array_keys($uploads));
  }

  public function AgentAdd($jobId, $uploadId, &$errorMsg, $dependencies=array(), $arguments=null)
  {
    $dependencies[] = "agent_pkgagent";
    $dependencies[] = "agent_copyright";
    $dependencies[] = "agent_ecc";
    $dependencies[] = "agent_ninka";
    $dependencies[] = "agent_mimetype";
    $dependencies[] = "agent_monk";
    $dependencies[] = "agent_nomos";

    if ($this->AgentHasResults($uploadId) == 1)
    {
      return 0;
    }

    $jobQueueId = \IsAlreadyScheduled($jobId, $this->AgentName, $uploadId);
    if ($jobQueueId != 0)
    {
      return $jobQueueId;
    }

    $args = is_array($arguments) ? '' : $arguments;
    return $this->doAgentAdd($jobId, $uploadId, $errorMsg, $dependencies, $uploadId, $args);
  }
}

register_plugin(new SpdxTwoAgentPlugin());
