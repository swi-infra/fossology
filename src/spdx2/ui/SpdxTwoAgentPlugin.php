<?php
/*
 SPDX-FileCopyrightText: © 2015 Siemens AG

 SPDX-License-Identifier: GPL-2.0-only
*/
/**
 * @dir
 * @brief UI for SPDX2 agent
 */
namespace Fossology\SpdxTwo\UI;

use Fossology\Lib\Plugin\AgentPlugin;

/**
 * @class SpdxTwoAgentPlugin
 * @brief Generate SPDX2 report for multiple uploads
 */
class SpdxTwoAgentPlugin extends AgentPlugin
{
  public function __construct()
  {
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
