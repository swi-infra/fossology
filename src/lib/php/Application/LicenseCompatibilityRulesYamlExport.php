<?php
/*
 Copyright (C) 2020, Siemens AG
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

namespace Fossology\Lib\Application;

use Fossology\Lib\BusinessRules\LicenseMap;
use Fossology\Lib\Db\DbManager;

/**
 * @file
 * @brief Helper class to export license list as a YAML from the DB
 */

/**
 * @class LicenseCompatibilityRulesYamlExport
 * @brief Helper class to export license list as a YAML from the DB
 */
class LicenseCompatibilityRulesYamlExport
{
  /** @var DbManager $dbManager
   * DB manager in use */
  protected $dbManager;
  /**
   * Constructor
   * @param DbManager $dbManager DB manager to use.
   */
  public function __construct(DbManager $dbManager)
  {
      $this->dbManager = $dbManager;
  }
  /**
   * @brief Create the YAML from the DB
   * @param int $lf Set the license ID to get only one license, set 0 to get all
   * @return string yaml
   */
  public function createYaml($lr=0)
  {
    $sql = "SELECT lrm.rf_shortname AS mainname,
            lrs.rf_shortname AS subname, main_type AS maintype, sub_type AS subtype, case compatibility when 't' then 'true'
            else 'false' end AS compatibility, text
            FROM license_rules r
            LEFT JOIN license_ref lrm ON lrm.rf_pk = r.main_rf_fk
            LEFT JOIN license_ref lrs ON lrs.rf_pk = r.sub_rf_fk";
    $param = array($lr);
    if ($lr > 0) {
      $stmt = __METHOD__ . '.lr';
      $param[] = $lr;
      $sql .= ' WHERE lr_pk = $'.count($param);
      $row = $this->dbManager->getSingleRow($sql,$param,$stmt);
      $vars = $row ? array( $row ) : array();
    } else {
        $stmt = __METHOD__;
        $sql .= ' ORDER BY lr_pk';
        $vars = $this->dbManager->getRows($sql,[],$stmt);
    }
    $rules =[];
    $rules["default"] = false;
    $rules["rules"] = $vars;
    return yaml_emit($rules);
  }
}
