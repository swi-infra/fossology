<?php
/*
 Copyright (C) 2017, Siemens AG

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

use Fossology\Lib\Dao\LicenseDao;
use Fossology\Lib\Dao\ClearingDao;
use Fossology\Lib\Dao\UploadDao;

class ObligationsToLicenses
{
  /** @var licenseDao */
  private $licenseDao;

  /** @var clearingDao */
  private $clearingDao;

  /** @var uploadDao */
  private $uploadDao;

  public function __construct()
  {
    global $container; 
    $this->licenseDao = $container->get('dao.license');
    $this->clearingDao = $container->get('dao.clearing');
    $this->uploadDao = $container->get('dao.upload');
  } 
  
  /**
   * @param array
   * @param array
   * @return array
  **/
  function getObligations($licenseStatements, $mainLicenseStatements, $uploadId, $groupId)
  {
    $licenseIds = $this->contentOnly($licenseStatements);
    $mainLicenseIds = $this->contentOnly($mainLicenseStatements);

    if(!empty($mainLicenseIds)){
      $allLicenseIds = array_unique(array_merge($licenseIds, $mainLicenseIds));
    }
    else{
      $allLicenseIds = array_unique($licenseIds);
    }

    $bulkAddIds = $this->getBulkAddLicenseList($uploadId, $groupId);
    $obligations = $this->licenseDao->getLicenseObligations($allLicenseIds);
    $onlyLicenseIdsWithObligation = array_column($obligations, 'rf_fk');
    if(!empty($bulkAddIds)){
      $onlyLicenseIdsWithObligation = array_unique(array_merge($onlyLicenseIdsWithObligation, $bulkAddIds));
    }
    $licenseWithoutObligations = array_diff($allLicenseIds, $onlyLicenseIdsWithObligation);
    foreach($licenseWithoutObligations as $licenseWithoutObligation){
      $license = $this->licenseDao->getLicenseById($licenseWithoutObligation);
      if(!empty($license)){
        $whiteLists[] = $license->getShortName();
      }
    }
    $newobligations = $this->groupObligations($obligations);
    return array($newobligations, $whiteLists);
  }

  function getBulkAddLicenseList($uploadId, $groupId)
  {
    $uploadTreeTableName = $this->uploadDao->getUploadtreeTableName($uploadId);
    $parentTreeBounds = $this->uploadDao->getParentItemBounds($uploadId, $uploadTreeTableName);
    $bulkHistory = $this->clearingDao->getBulkHistory($parentTreeBounds, $groupId, false);
    if(!empty($bulkHistory)){
      $licenseLists = array_column($bulkHistory, 'addedLicenses');
      $allLicenses = array();
      foreach($licenseLists as $licenseList){
        $allLicenses = array_unique(array_merge($allLicenses, $licenseList));
      }
      foreach($allLicenses as $allLicense){
        $license = $this->licenseDao->getLicenseByShortName($allLicense);
        if(!empty($license)){
          $licenseId[] = $license->getId();
        }
      }
    }
    return $licenseId;
  }

  function groupObligations($obligations)
  {
    foreach($obligations as $obligation ) {
      $groups[$obligation['ob_pk']][] = $obligation;
    }
    foreach($groups as $group){
      for($i=1; $i <= sizeof($group)+1; $i++)
      {
        if(strcmp($group[0]['ob_topic'], $group[$i]['ob_topic'])==0){
          if(strcmp($group[0]['ob_text'], $group[$i]['ob_text'])==0){
            $group[0]['rf_shortname'] = $group[0]['rf_shortname'].','.$group[$i]['rf_shortname'];
            unset($group[$i]);
          }
        }
      }
      $newGroup[] = $group;
    }
    return $newGroup;
  }

  /**
   * @param array
   * @return array
  **/
  function contentOnly($licenseStatements)
  {
    foreach($licenseStatements as $licenseStatement){
      $licenseId[] = $licenseStatement["licenseId"];
    }
    return $licenseId;
  }
}
