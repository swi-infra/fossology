/***************************************************************
 wget_agent: Retrieve a file and put it in the database.

 Copyright (C) 2007-2014 Hewlett-Packard Development Company, L.P.

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

 ***************************************************************/

/**
 * \file wget_agent.c
 * \brief wget_agent: Retrieve a file and put it in the database.
 */

#define _GNU_SOURCE           // for asprintf

#include "wget_agent.h"

#define EXIT_MEM_ERROR  -999

char SQL[MAXCMD];

PGconn *pgConn = NULL;        ///< For the DB

global_vars g;

gid_t ForceGroup=-1;          ///< Set to group id to be used for download files

/**
 * \brief Given a filename, is it a file?
 * \param Link Should it follow symbolic links?
 * \return int 1=yes, 0=no.
 */
int IsFile(char *Fname, int Link)
{
  stat_t Stat;
  int rc;
  if (!Fname || (Fname[0]=='\0')) return(0);  /* not a directory */
  if (Link) rc = stat64(Fname,&Stat);
  else rc = lstat64(Fname,&Stat);
  if (rc != 0) return(0); /* bad name */
  return(S_ISREG(Stat.st_mode));
} /* IsFile() */

/**
 * \brief Closes the connection to the server, free the database connection, and exit.
 * \param rc Exit value
 */
void  SafeExit(int rc)
{
  if (pgConn) PQfinish(pgConn);
  fo_scheduler_disconnect(rc);
  exit(rc);
} /* SafeExit() */

/**
 * \brief Get the position (ending + 1) of http|https|ftp:// of one url
 * \param URL The URL
 * \return the position (ending + 1) of http|https|ftp:// of one url
 *         E.g. http://fossology.org, return 7
 */
int GetPosition(char *URL)
{
  if (NULL != strstr(URL, "http://"))  return 7;
  if (NULL != strstr(URL, "https://"))  return 8;
  if (NULL != strstr(URL, "ftp://"))  return 6;
  return 0;
}

/**
 * \brief Insert a file into the database and repository.
 *
 * Copy the downloaded file to gold repository according to
 * the checksum and change the owner if ForceGroup is set.
 * Then insert in upload and pfile tables.
 */
void DBLoadGold()
{
  Cksum *Sum;
  char *Unique=NULL;
  char *SHA1, *MD5, *Len;
  char SQL[MAXCMD];
  long PfileKey;
  char *Path;
  char SHA256[65], command[PATH_MAX + 13];
  FILE *Fin;
  int rc = -1, i;
  PGresult *result;
  int read = 0;

  memset(SHA256, '\0', sizeof(SHA256));

  LOG_VERBOSE0("Processing %s",g.temp_file);
  Fin = fopen(g.temp_file,"rb");

  // Calculate sha256 value
  snprintf(command, PATH_MAX + 13, "sha256sum '%s'", g.temp_file);
  FILE* file = popen(command, "r");

  if (!Fin)
  {
    LOG_FATAL("upload %ld Unable to open temp file %s from %s",
        g.upload_key,g.temp_file,g.URL);
    SafeExit(1);
  }

  if (file)
  {
    read = fscanf(file, "%64s", SHA256);
    rc = WEXITSTATUS(pclose(file));

    if (rc || read != 1)
    {
      LOG_FATAL("Unable to calculate SHA256 of %s\n", g.temp_file);
      SafeExit(56);
    }
  }

  // Change SHA256 to upper case like other checksums
  for (i = 0; i < 65; i++)
  {
    SHA256[i] = toupper(SHA256[i]);
  }

  Sum = SumComputeFile(Fin);
  fclose(Fin);

  if ((int)ForceGroup > 0)
  {
    rc = chown(g.temp_file,-1,ForceGroup);
    if (rc) LOG_ERROR("chown failed on %s, error: %s", g.temp_file, strerror(errno));
  }

  if (!Sum)
  {
    LOG_FATAL("upload %ld Unable to compute checksum for %s from %s",
        g.upload_key,g.temp_file,g.URL);
    SafeExit(2);
  }

  if (Sum->DataLen <= 0)
  {
    LOG_FATAL("upload %ld No bytes downloaded from %s to %s.",
        g.upload_key,g.URL,g.temp_file);
    SafeExit(3);
  }

  Unique = SumToString(Sum);
  LOG_VERBOSE0("Unique %s",Unique);

  if (g.import_gold)
  {
    LOG_VERBOSE0("Import Gold %s",Unique);
    rc = fo_RepImport(g.temp_file,"gold",Unique,1);
    if (rc)
    {
      LOG_FATAL("upload %ld Failed to import %s from %s into repository gold %s",
          g.upload_key,g.temp_file,g.URL,Unique);
      SafeExit(4);
    }
    /* Put the file in the "files" repository too */
    Path = fo_RepMkPath("gold",Unique);
    if ((int)ForceGroup >= 0)
    {
      rc = chown(Path,-1,ForceGroup);
      if (rc) LOG_ERROR("chown failed on %s, error: %s", Path, strerror(errno));
    }
  } /* if import_gold */
  else /* if !import_gold */
  {
    Path = g.temp_file;
  } /* else if !import_gold */

  LOG_VERBOSE0("Path is %s",Path);

  if (!Path)
  {
    LOG_FATAL("upload %ld Failed to determine repository location for %s in gold",
        g.upload_key,Unique);
    SafeExit(5);
  }

  LOG_VERBOSE0("Import files %s",Path);
  if (fo_RepImport(Path,"files",Unique,1))
  {
    LOG_FATAL("upload %ld Failed to import %s from %s into files",
        g.upload_key,Unique,Path);
    SafeExit(6);
  }

  if ((int)ForceGroup >= 0)
  {
    rc = chown(Path,-1,ForceGroup);
    if (rc) LOG_ERROR("chown failed on %s, error: %s", Path, strerror(errno));
  }

  if (Path != g.temp_file)
  {
    if(Path)
    {
      free(Path);
      Path = NULL;
    }
  }

  /* Now update the DB */
  /* Break out the sha1, md5, len components **/
  SHA1 = Unique;
  MD5 = Unique+41; /* 40 for sha1 + 1 for '.' */
  Len = Unique+41+33; /* 32 for md5 + 1 for '.' */
  /* Set the pfile */
  memset(SQL,'\0',MAXCMD);
  snprintf(SQL,MAXCMD-1,"SELECT pfile_pk FROM pfile WHERE pfile_sha1 = '%.40s' AND pfile_md5 = '%.32s' AND pfile_size = %s;",
      SHA1,MD5,Len);
  result =  PQexec(pgConn, SQL); /* SELECT */

  if (fo_checkPQresult(pgConn, result, SQL, __FILE__, __LINE__)) {
    SafeExit(7);
  }

  /* See if pfile needs to be added */
  if (PQntuples(result) <= 0)
  {
    /* Insert it */
    memset(SQL,'\0',MAXCMD);
    snprintf(SQL,MAXCMD-1,"INSERT INTO pfile (pfile_sha1, pfile_md5, pfile_sha256, pfile_size) VALUES ('%.40s','%.32s','%.64s',%s)",
        SHA1,MD5,SHA256,Len);
    PQclear(result);
    result = PQexec(pgConn, SQL);
    if (fo_checkPQcommand(pgConn, result, SQL, __FILE__, __LINE__)) SafeExit(8);
    PQclear(result);
    result = PQexec(pgConn, "SELECT currval('pfile_pfile_pk_seq')");
    if (fo_checkPQresult(pgConn, result, SQL, __FILE__, __LINE__)) SafeExit(182);
  }

  PfileKey = atol(PQgetvalue(result,0,0));
  LOG_VERBOSE0("pfile_pk = %ld",PfileKey);

  /* Update the DB so the pfile is linked to the upload record */
  PQclear(result);
  result = PQexec(pgConn, "BEGIN");
  if (fo_checkPQcommand(pgConn, result, SQL, __FILE__, __LINE__)) SafeExit(-1);

  memset(SQL,0,MAXCMD);
  snprintf(SQL,MAXCMD-1,"SELECT * FROM upload WHERE upload_pk=%ld FOR UPDATE;",g.upload_key);
  PQclear(result);
  result = PQexec(pgConn, SQL);
  if (fo_checkPQresult(pgConn, result, SQL, __FILE__, __LINE__)) SafeExit(-1);

  memset(SQL,0,MAXCMD);
  snprintf(SQL,MAXCMD-1,"UPDATE upload SET pfile_fk=%ld WHERE upload_pk=%ld",
      PfileKey,g.upload_key);
  LOG_VERBOSE0("SQL=%s\n",SQL);
  PQclear(result);
  result = PQexec(pgConn, SQL);
  if (fo_checkPQcommand(pgConn, result, SQL, __FILE__, __LINE__)) SafeExit(9);
  PQclear(result);
  result = PQexec(pgConn, "COMMIT;");
  if (fo_checkPQcommand(pgConn, result, SQL, __FILE__, __LINE__)) SafeExit(92);
  PQclear(result);

  /* Clean up */
  if (Sum)
  {
    free(Sum);
    Sum = NULL;
  }
  if (Unique)
  {
    free(Unique);
    Unique = NULL;
  }
} /* DBLoadGold() */


/**
 * \brief Given a URL string, taint-protect it.
 * \param[in]  Sin The source URL
 * \param[out] Sout The tainted URL
 * \param[in]  SoutSize The capacity of Sout
 * \return 1=tainted, 0=failed to taint
 */
int TaintURL(char *Sin, char *Sout, int SoutSize)
{
  int i;
  int si;
  memset(Sout,'\0',SoutSize);
  SoutSize--; /* always keep the EOL */
  for(i=0,si=0; (si<SoutSize) && (Sin[i] != '\0'); i++)
  {
    if (Sin[i] == '#') return(0);  /* end at the start of comment */
    if (!strchr("'`",Sin[i]) && !isspace(Sin[i])) {
      Sout[si++] = Sin[i];
    } else {
      if (si+3 >= SoutSize) return(0); /* no room */
      snprintf(Sout+si,4,"%%%02X",Sin[i]);
      si+=3;
    }
  }

  return(Sin[i]=='\0');
} /* TaintURL() */

/**
 * \brief Do the wget.
 * \param TempFile
 * \parblock
 * Used when upload from URL by the scheduler, the downloaded file(directory) will be archived as this file.
 * When running from command, this parameter is null, e.g. /srv/fossology/repository/localhost/wget/wget.32732
 * \endparblock
 * \param URL The url you want to download
 * \param TempFileDir Where you want to store your downloaded file(directory)
 * \return int, 0 on success, non-zero on failure.
 */
int GetURL(char *TempFile, char *URL, char *TempFileDir)
{
  char *cmd;
  char TaintedURL[MAXCMD];
  char *tmp_file_dir;
  char *del_tmp_dir_cmd;
  int rc;

  /* save each upload files in /srv/fossology/repository/localhost/wget/wget.xxx.dir/ */
  if (asprintf(&tmp_file_dir, "%s.dir", TempFile) == -1)
  {
    SafeExit(EXIT_MEM_ERROR);
  }

  if (asprintf(&del_tmp_dir_cmd, "rm -rf %s", tmp_file_dir) == -1)
  {
    free(tmp_file_dir);
    SafeExit(EXIT_MEM_ERROR);
  }
#if 1
  char WgetArgs[]="--no-check-certificate --progress=dot -rc -np -e robots=off";
#else
  /* wget < 1.10 does not support "--no-check-certificate" */
  char WgetArgs[]="--progress=dot -rc -np -e robots=off";
#endif

  if (!TaintURL(URL,TaintedURL,MAXCMD))
  {
    LOG_FATAL("Failed to taint the URL '%s'",URL);
    free(del_tmp_dir_cmd);
    free(tmp_file_dir);
    SafeExit(10);
  }

  /*
   Wget options:
   --progress=dot :: display a new line as it progresses.
   --no-check-certificate :: download HTTPS files even if the cert cannot
     be validated.  (Neal has many issues with SSL and does not view it
     as very secure.)  Without this, some caching proxies and web sites
     with old certs won't download.  Granted, in theory a bad cert should
     prevent downloads.  In reality, 99.9% of bad certs are because the
     admin did not notice that they expired and not because of a hijacking
     attempt.
   */

  struct stat sb;
  int rc_system =0;
  char no_proxy[MAXCMD] = {0};
  char proxy[MAXCMD] = {0};
  char proxy_temp[MAXCMD] = {0};

  /* http_proxy is optional so don't error if it doesn't exist */
  /** set proxy */
  if (g.proxy[0] && g.proxy[0][0])
  {
    snprintf(proxy_temp, MAXCMD-1, "export http_proxy='%s' ;", g.proxy[0]);
    strcat(proxy, proxy_temp);
  }
  if (g.proxy[1] && g.proxy[1][0])
  {
    snprintf(proxy_temp, MAXCMD-1, "export https_proxy='%s' ;", g.proxy[1]);
    strcat(proxy, proxy_temp);
  }
  if (g.proxy[2] && g.proxy[2][0])
  {
    snprintf(proxy_temp, MAXCMD-1, "export ftp_proxy='%s' ;", g.proxy[2]);
    strcat(proxy, proxy_temp);
  }
  if (g.proxy[3] && g.proxy[3][0])
  {
    snprintf(no_proxy, MAXCMD-1, "-e no_proxy='%s'", g.proxy[3]);
  }

  if (TempFile && TempFile[0])
  {
    /* Delete the temp file if it exists */
    unlink(TempFile);
    if (asprintf(&cmd, " %s /usr/bin/wget -q %s -P '%s' '%s' %s %s 2>&1",
          proxy, WgetArgs, tmp_file_dir, TaintedURL, g.param, no_proxy) == -1)
    {
      free(del_tmp_dir_cmd);
      free(tmp_file_dir);
      SafeExit(EXIT_MEM_ERROR);
    }
  }
  else if(tmp_file_dir[0])
  {
    if (asprintf(&cmd, " %s /usr/bin/wget -q %s -P '%s' '%s' %s %s 2>&1",
        proxy, WgetArgs, tmp_file_dir, TaintedURL, g.param, no_proxy) == -1)
    {
      free(del_tmp_dir_cmd);
      free(tmp_file_dir);
      SafeExit(EXIT_MEM_ERROR);
    }
  }
  else
  {
    if (asprintf(&cmd, " %s /usr/bin/wget -q %s '%s' %s %s 2>&1",
        proxy, WgetArgs, TaintedURL, g.param, no_proxy) == -1)
    {
      free(del_tmp_dir_cmd);
      free(tmp_file_dir);
      SafeExit(EXIT_MEM_ERROR);
    }
  }

  /* the command is like
  ". /usr/local/etc/fossology/Proxy.conf;
     /usr/bin/wget -q --no-check-certificate --progress=dot -rc -np -e robots=off -P
     '/srv/fossology/repository/localhost/wget/wget.xxx.dir/'
     'http://a.org/file' -l 1 -R index.html*  2>&1"
   */
  LOG_VERBOSE0("CMD: %s", cmd);
  // XXX
  printf("DEBUG - REMOVE ME - CMD: %s", cmd);
  rc = system(cmd);

  if (WIFEXITED(rc) && (WEXITSTATUS(rc) != 0))
  {
    LOG_FATAL("upload %ld Download failed; Return code %d from: %s",g.upload_key,WEXITSTATUS(rc),cmd);
    free(cmd);
    unlink(g.temp_file);
    rc_system = system(del_tmp_dir_cmd);
    if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, del_tmp_dir_cmd)

    free(del_tmp_dir_cmd);
    free(tmp_file_dir);
    SafeExit(12);
  }

  free(cmd);

  if (!(TempFile && TempFile[0]))
  {
    /* remove the temp dir /srv/fossology/repository/localhost/wget/wget.xxx.dir/ for this upload */
    rc_system = system(del_tmp_dir_cmd);
    if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, del_tmp_dir_cmd)
    LOG_VERBOSE0("upload %ld Downloaded %s to %s",g.upload_key,URL,TempFile);

    free(del_tmp_dir_cmd);
    free(tmp_file_dir);
    return(0);
  }

  /* Run from scheduler! store /srv/fossology/repository/localhost/wget/wget.xxx.dir/<files|directories> to one temp file */
  char* tmp_file_path;

  /* for one url http://a.org/test.deb, TempFilePath should be /srv/fossology/repository/localhost/wget/wget.xxx.dir/a.org/test.deb */
  int Position = GetPosition(TaintedURL);
  if (0 == Position)
  {
    LOG_FATAL("path %s is not http://, https://, or ftp://", TaintedURL);
    unlink(g.temp_file);
    rc_system = system(del_tmp_dir_cmd);
    if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, del_tmp_dir_cmd)

    free(del_tmp_dir_cmd);
    free(tmp_file_dir);
    SafeExit(26);
  }

  if (asprintf(&tmp_file_path, "%s/%s", tmp_file_dir, TaintedURL + Position) == -1)
  {
    free(del_tmp_dir_cmd);
    free(tmp_file_dir);
    SafeExit(EXIT_MEM_ERROR);
  }

  if (!stat(tmp_file_path, &sb))
  {
    if (S_ISDIR(sb.st_mode))
    {
      if (asprintf(&cmd, "find '%s' -mindepth 1 -type d -empty -exec rmdir {} \\; > /dev/null 2>&1", tmp_file_path) == -1)
      {
        free(del_tmp_dir_cmd);
        free(tmp_file_dir);
        free(tmp_file_path);
        SafeExit(EXIT_MEM_ERROR);
      }

      rc_system = system(cmd); // delete all empty directories downloaded
      if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, cmd)

      free(cmd);
      if (asprintf(&cmd, "tar -cf  '%s' -C '%s' ./ 1>/dev/null", TempFile, tmp_file_path) == -1)
      {
        free(del_tmp_dir_cmd);
        free(tmp_file_dir);
        free(tmp_file_path);
        SafeExit(EXIT_MEM_ERROR);
      }
    }
    else
    {
      if (asprintf(&cmd, "mv '%s' '%s' 2>&1", tmp_file_path, TempFile) == -1)
      {
        free(del_tmp_dir_cmd);
        free(tmp_file_dir);
        free(tmp_file_path);
        SafeExit(EXIT_MEM_ERROR);
      }
    }

    free(tmp_file_path);

    rc_system = system(cmd);
    if (rc_system != 0)
    {
      systemError(__LINE__, rc_system, cmd)
      unlink(g.temp_file);
      rc_system = system(del_tmp_dir_cmd);
      if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, del_tmp_dir_cmd)

      free(del_tmp_dir_cmd);
      free(tmp_file_dir);
      free(cmd);
      SafeExit(24); // failed to store the temperary directory(one file) as one temperary file
    }
  }
  else
  {
    if (asprintf(&cmd, "find '%s' -type f -exec mv {} %s \\; > /dev/null 2>&1", tmp_file_dir, TempFile) == -1)
    {
      free(del_tmp_dir_cmd);
      free(tmp_file_dir);
      SafeExit(EXIT_MEM_ERROR);
    }
    rc_system = system(cmd);
    if (rc_system != 0)
    {
      systemError(__LINE__, rc_system, cmd)
      unlink(g.temp_file);
      rc_system = system(del_tmp_dir_cmd);
      if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, del_tmp_dir_cmd)

      free(del_tmp_dir_cmd);
      free(tmp_file_dir);
      free(cmd);
      SafeExit(24); // failed to store the temperary directory(one file) as one temperary file
    }
  }

  free(tmp_file_dir);
  if (!IsFile(TempFile,1))
  {
    LOG_FATAL("upload %ld File %s not created from URL: %s, CMD: %s",g.upload_key,TempFile,URL, cmd);
    unlink(g.temp_file);
    rc_system = system(del_tmp_dir_cmd);
    if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, del_tmp_dir_cmd)
    free(del_tmp_dir_cmd);
    free(cmd);
    SafeExit(15);
  }

  free(cmd);
  /* remove the temp dir /srv/fossology/repository/localhost/wget/wget.xxx.dir/ for this upload */
  rc_system = system(del_tmp_dir_cmd);
  if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, del_tmp_dir_cmd)
  LOG_VERBOSE0("upload %ld Downloaded %s to %s",g.upload_key,URL,TempFile);

  free(del_tmp_dir_cmd);
  return(0);
} /* GetURL() */

/**
 * \brief Get source code from version control system
 * \return int - 0: successful; others: fail
 */
int GetVersionControl()
{
  char *command = NULL;
  char *tmp_file_dir;
  char *delete_tmp_dir_cmd;
  char *tmp_home;  
  int rc = 0;
  int resethome = 0; // 0: default; 1: home is null before setting, should rollback
  int rc_system =0;
  char *homeenv = NULL;

  homeenv = getenv("HOME");
  if(NULL == homeenv) resethome = 1;

  /* We need HOME to point to where .gitconfig is installed
   * path is the repository path and .gitconfig is installed in its parent directory
   */
  if (asprintf(&tmp_home, "%s/..", fo_config_get(sysconfig, "FOSSOLOGY", "path", NULL)) == -1)
  {
    return -1;
  }
  setenv("HOME", tmp_home, 1);
  free(tmp_home);

  /* save each upload files in /srv/fossology/repository/localhost/wget/wget.xxx.dir/ */
  if (asprintf(&tmp_file_dir, "%s.dir", g.temp_file) == -1)
  {
    return -1;
  }

  if (asprintf(&delete_tmp_dir_cmd, "rm -rf %s", tmp_file_dir) == -1)
  {
    free(tmp_file_dir);
    return -1;
  }

  command = GetVersionControlCommand(1);
  if (!command) {
    free(tmp_file_dir);
    free(delete_tmp_dir_cmd);
    return -1;
  }

  rc = system(command);
  free(command);

  if (resethome) { // rollback
    unsetenv("HOME");
  } else {
    setenv("HOME", homeenv, 1);
  }

  if (rc != 0)
  {
    command = GetVersionControlCommand(-1);
    if (!command)
    {
      free(tmp_file_dir);
      free(delete_tmp_dir_cmd);
      return -1;
    }
    systemError(__LINE__, rc, command)
    /** for user fossy
    \code git: git config --global http.proxy web-proxy.cce.hp.com:8088; git clone http://github.com/schacon/grit.git
    svn: svn checkout --config-option servers:global:http-proxy-host=web-proxy.cce.hp.com --config-option servers:global:http-proxy-port=8088 https://svn.code.sf.net/p/fossology/code/trunk/fossology/utils/ \endcode
    */
    LOG_FATAL("please make sure the URL of repo is correct, also add correct proxy for your version control system, command is:%s, g.temp_file is:%s, rc is:%d. \n", command, g.temp_file, rc);
    /* remove the temp dir /srv/fossology/repository/localhost/wget/wget.xxx.dir/ for this upload */
    rc_system = system(delete_tmp_dir_cmd);
    if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, delete_tmp_dir_cmd)
    
    free(tmp_file_dir);
    free(delete_tmp_dir_cmd);
    free(command);
    return 1;
  }

  if (asprintf(&command, "tar -cf  '%s' -C '%s' ./ 1>/dev/null", g.temp_file, tmp_file_dir) == -1)
  {
    free(tmp_file_dir);
    free(delete_tmp_dir_cmd);
    return 1;
  }
    
  free(tmp_file_dir);

  rc = system(command);

  if (rc != 0)
  {
    systemError(__LINE__, rc, command)
    /* remove the temp dir /srv/fossology/repository/localhost/wget/wget.xxx.dir/ for this upload */
    rc_system = system(delete_tmp_dir_cmd);
    if (!WIFEXITED(rc_system)) systemError(__LINE__, rc_system, delete_tmp_dir_cmd)
    LOG_FATAL("DeleteTempDirCmd is:%s\n", delete_tmp_dir_cmd);

    free(command);
    free(delete_tmp_dir_cmd);
    return 1;
  }

  free(command);

  /* remove the temp dir /srv/fossology/repository/localhost/wget/wget.xxx.dir/ for this upload */
  rc_system = system(delete_tmp_dir_cmd);
  if (!WIFEXITED(rc_system)) {
    systemError(__LINE__, rc_system, delete_tmp_dir_cmd)
  }

  free(delete_tmp_dir_cmd);

  return 0; // succeed to retrieve source
}

/**
 * \brief Convert input pairs into globals.
 *
 * This functions taints the parameters as needed.
 * \param S The parameters for wget_aget have 2 parts, one is from scheduler, that is S
 * \param TempFileDir The parameters for wget_aget have 2 parts, one is from wget_agent.conf, that is TempFileDir
 */
void    SetEnv  (char *S, char *TempFileDir)
{
  int SLen,GLen; /* lengths for S and global string */

  g.upload_key = -1;
  memset(g.temp_file,'\0',MAXCMD);
  memset(g.URL,'\0',MAXCMD);
  if (!S) return;

  /* first value is the upload_pk */
  g.upload_key = atol(S);
  while(S[0] && isdigit(S[0])) S++;
  while(S[0] && isspace(S[0])) S++; /* skip spaces */

#if 1
  /* second value is the temp file location */
  /** This will be removed when the jobqueue is changed. **/
  SLen=0;
  GLen=0;
  while((GLen < MAXCMD-4) && S[SLen] && !isspace(S[SLen]))
  {
    if ((S[SLen] == '\'') || isspace(S[SLen]) || !isprint(S[SLen]))
    {
      sprintf(g.temp_file+GLen,"%%%02x",(unsigned char)(S[SLen]));
      GLen += 3;
    }
    else g.temp_file[GLen++] = S[SLen];
    SLen++;
  }
  S+=SLen;
  while(S[0] && isspace(S[0])) S++; /* skip spaces */
#endif
  if (TempFileDir)
  {
    memset(g.temp_file,'\0',MAXCMD);
    snprintf(g.temp_file,MAXCMD-1,"%s/wget.%d",TempFileDir,getpid());
  }

  /* third value is the URL location -- taint any single-quotes */
  SLen=0;
  GLen=0;
  while((GLen < MAXCMD-4) && S[SLen])
  {
    if ((S[SLen] == '\\') && isprint(S[SLen+1])) // in file path, if include '\ ', that mean this file name include spaces
    {
      LOG_FATAL("S[SLen] is:%c\n", S[SLen]);
      g.URL[GLen++] = ' ';
      SLen += 2;
      continue;
    }
    else if ((S[SLen] != '\\') && isspace(S[SLen])) break;
    else if ((S[SLen] == '\'') || isspace(S[SLen]) || !isprint(S[SLen]))
    {
      sprintf(g.URL+GLen,"%%%02x",(unsigned char)(S[SLen]));
      GLen += 3;
    }
    else g.URL[GLen++] = S[SLen];
    SLen++;
  }
  S+=SLen;

  while(S[0] && isspace(S[0])) S++; /* skip spaces */

  char Type[][4] = {"SVN", "Git", "CVS"};
  int i = 0; // type index

  memset(g.type,'\0',MAXCMD);
  strncpy(g.type, S, 3);
  if ((0 == strcmp(g.type, Type[i++])) || (0 == strcmp(g.type, Type[i++])) || (0 == strcmp(g.type, Type[i++])))
  {
    S += 3;
  }
  else
  {
    memset(g.type,'\0',MAXCMD);
  }

  strncpy(g.param, S, sizeof(g.param)); // get the parameters, kind of " -A rpm -R fosso -l 1* "
  LOG_VERBOSE0("  upload %ld wget_agent globals loaded:\n  upload_pk = %ld\n  tmpfile=%s  URL=%s  g.param=%s\n",g.upload_key, g.upload_key,g.temp_file,g.URL,g.param);
} /* SetEnv() */


/**
 * \brief Check if path contains a "%H", "%R".
 *
 * Substitute the "%H" with hostname and "%R" with repo location.
 * \parm DirPath Directory path.
 * \returns new directory path
 */
char *PathCheck(char *DirPath)
{
  char *NewPath;
  char *subs;
  char  TmpPath[2048];
  char  HostName[2048];

  NewPath = strdup(DirPath);

  if ((subs = strstr(NewPath,"%H")) )
  {
    /* hostname substitution */
    gethostname(HostName, sizeof(HostName));

    *subs = 0;
    snprintf(TmpPath, sizeof(TmpPath), "%s%s%s", NewPath, HostName, subs+2);
    free(NewPath);
    NewPath = strdup(TmpPath);
  }

  if ((subs = strstr(NewPath, "%R")) )
  {
    /* repo location substitution */
    *subs = 0;

    snprintf(TmpPath, sizeof(TmpPath), "%s%s%s", NewPath, fo_config_get(sysconfig, "FOSSOLOGY", "path", NULL), subs+2);
    free(NewPath);
    NewPath = strdup(TmpPath);
  }

  return(NewPath);
}

/**
 * \brief Copy downloaded files to temporary directory
 *
 * If the path(fs) is a directory, create a tar file from files(dir) in a directory
 * to the temporary directory.
 *
 * If the path(fs) is a file, copy the file to the temporary directory
 * \param Path The fs will be handled, directory(file) you want to upload from server
 * \param TempFile The tar(regular) file name
 * \param TempFileDir Temporary directory path
 * \param Status The status of Path
 *
 * \return 1 on sucess, 0 on failure
 */
int Archivefs(char *Path, char *TempFile, char *TempFileDir, struct stat Status)
{
  char *cmd;
  int rc_system = 0;

  if (asprintf(&cmd, "mkdir -p '%s' >/dev/null 2>&1", TempFileDir) == -1)
  {
    return 0;
  }

  rc_system = system(cmd);
  if (!WIFEXITED(rc_system))
  {
    LOG_FATAL("[%s:%d] Could not create temporary directory", __FILE__, __LINE__);
    systemError(__LINE__, rc_system, cmd)
    free(cmd);
    return 0;
  }

  free(cmd);

  if (S_ISDIR(Status.st_mode)) /* directory? */
  {
    if (asprintf(&cmd, "tar %s -cf  '%s' -C '%s' ./ 1>/dev/null", g.param, TempFile, Path) == -1)
    {
      return 0;
    }
    rc_system = system(cmd);
    if (!WIFEXITED(rc_system))
    {
      systemError(__LINE__, rc_system, cmd)
      free(cmd);
      return 0;
    }
    free(cmd);
  } else if (strstr(Path, "*"))  // wildcards
  {

    /* for the wildcards upload, keep the path */
    /* copy * files to TempFileDir/temp primarily */
    if (asprintf(&cmd, "mkdir -p '%s/temp'  > /dev/null 2>&1 && cp -r %s '%s/temp' > /dev/null 2>&1", TempFileDir, Path, TempFileDir) == -1)
    {
      return 0;
    }
    rc_system = system(cmd);
    if (rc_system != 0)
    {
      systemError(__LINE__, rc_system, cmd)
      free(cmd);
      return 0;
    }

    free(cmd);

    if (asprintf(&cmd, "tar -cf  '%s' -C %s/temp ./  1> /dev/null && rm -rf %s/temp  > /dev/null 2>&1", TempFile, TempFileDir, TempFileDir) == -1)
    {
      return 0;
    }
    rc_system = system(cmd);
    if (rc_system != 0)
    {
      systemError(__LINE__, rc_system, cmd)
      free(cmd);
      return 0;
    }

    free(cmd);
  } else if(S_ISREG(Status.st_mode)) /* regular file? */
  {

    if (asprintf(&cmd, "cp '%s' '%s' >/dev/null 2>&1", Path, TempFile) == -1)
    {
      return 0;
    }

    rc_system = system(cmd);
    if (rc_system != 0)
    {
      systemError(__LINE__, rc_system, cmd)
      free(cmd);
      return 0;
    }
    free(cmd);
  }

  return 0; /* neither a directory nor a regular file */
}

/**
 * \brief Get proxy from fossology.conf
 *
 * Get proxy from fossology.conf and copy in g.proxy array
 */
void GetProxy()
{
  int i = 0;
  int count_temp = 0;
  char *http_proxy_host = NULL;
  char *http_proxy_port = NULL;
  char *http_temp = NULL;

  for (i = 0; i < 6; i++)
  {
    g.proxy[i++] = NULL;
  }

  GError* error1 = NULL;
  GError* error2 = NULL;
  GError* error3 = NULL;
  GError* error4 = NULL;

  i = 0;
  g.proxy[i] = fo_config_get(sysconfig, "FOSSOLOGY", "http_proxy", &error1);
  trim(g.proxy[i++]);
  g.proxy[i] = fo_config_get(sysconfig, "FOSSOLOGY", "https_proxy", &error2);
  trim(g.proxy[i++]);
  g.proxy[i] = fo_config_get(sysconfig, "FOSSOLOGY", "ftp_proxy", &error3);
  trim(g.proxy[i++]);
  g.proxy[i] = fo_config_get(sysconfig, "FOSSOLOGY", "no_proxy", &error4);
  trim(g.proxy[i++]);

  if (g.proxy[0] && g.proxy[0][0])
  {
    http_proxy_port = strrchr(g.proxy[0], ':');
    strncpy(g.http_proxy, g.proxy[0], (http_proxy_port - g.proxy[0]));
    http_proxy_port++;

    if (http_proxy_port && http_proxy_port[0])
    {
      /* exclude '/' in http_proxy_port and 'http://' in http_proxy_host */
      http_temp = strchr(http_proxy_port, '/');
      if (http_temp && http_temp[0])
      {
        count_temp = http_temp - http_proxy_port;
        http_proxy_port[count_temp] = 0;
      }
      g.proxy[4] = g.http_proxy;
      g.proxy[5] = http_proxy_port;

      http_proxy_host = strrchr(g.http_proxy, '/');
      if (http_proxy_host && http_proxy_host[0])
      {
        http_proxy_host++;
        g.proxy[4] = http_proxy_host;
      }
    }
  }
}

/**
 * \brief Here are some suggested options
 * \param Name The name of the executable, usually it is wget_agent
 */
void Usage(char *Name)
{
  printf("Usage: %s [options] [OBJ]\n",Name);
  printf("  -h  :: help (print this message), then exit.\n");
  printf("  -i  :: Initialize the DB connection then exit (nothing downloaded)\n");
  printf("  -g group :: Set the group on processed files (e.g., -g fossy).\n");
  printf("  -G  :: Do NOT copy the file to the gold repository.\n");
  printf("  -d dir :: directory for downloaded file storage\n");
  printf("  -k key :: upload key identifier (number)\n");
  printf("  -A acclist :: Specify comma-separated lists of file name suffixes or patterns to accept.\n");
  printf("  -R rejlist :: Specify comma-separated lists of file name suffixes or patterns to reject.\n");
  printf("  -l depth :: Specify recursion maximum depth level depth.  The default maximum depth is 5.\n");
  printf("  -c configdir :: Specify the directory for the system configuration.\n");
  printf("  -C :: run from command line.\n");
  printf("  -v :: verbose (-vv = more verbose).\n");
  printf("  -V :: print the version info, then exit.\n");
  printf("  OBJ :: if a URL is listed, then it is retrieved.\n");
  printf("         if a file is listed, then it used.\n");
  printf("         if OBJ and Key are provided, then it is inserted into\n");
  printf("         the DB and repository.\n");
  printf("  no file :: process data from the scheduler.\n");
} /* Usage() */

 /**
  * \brief Translate authentication of git clone
  *
  * Translate authentication of git clone
  * from http://git.code.sf.net/p/fossology/fossology.git --username --password password (input)
  * to http://username:password@git.code.sf.net/p/fossology/fossology.git
  */
void replace_url_with_auth()
{
  const char needle[] = " ";
  const char needle2[] = "//";
  int index = 0;
  char *username = NULL;
  char *password = NULL;
  char http[10] = "";
  char URI[FILEPATH] = "";
  char *token = NULL;
  char *temp = NULL;

  if (!(strstr(g.param, "password") && strstr(g.param, "username")))
  {
    return;
  }

  temp = strstr(g.URL, needle2);
  strcpy(URI, temp + 2);
  strncpy(http, g.URL, strlen(g.URL) - strlen(URI));

  /* get the first token */
  token = strtok(g.param, needle);
  /* walk through other tokens */
  while( token != NULL )
  {
    if (1 == index) username = token;
    if (3 == index) password = token;
    token = strtok(NULL, needle);
    index++;
  }
  snprintf(g.URL, sizeof(g.URL)-1, "%s%s:%s@%s", http, username, password, URI);
  memset(g.param,'\0',MAXCMD);
}

/**
 * \brief Get the username from g.param and create new parameters without password
 */
void MaskPassword()
{
  const char needle[] = " ";
  int index = 0;
  int secondIndex = 0;
  char *username = NULL;
  char *token = NULL;
  char newParam[MAXCMD];
  char *beg = NULL;
  char *end = NULL;

  memset(newParam, '\0', MAXCMD);
  // SVN if parameters exists
  if (strstr(g.param, "password") && strstr(g.param, "username")) {
    /* get the first token */
    token = strtok(g.param, needle);
    /* walk through other tokens */
    while( token != NULL )
    {
      if (1 == index) {  //username is the first parameter
        username = token;
        break;
      }
      token = strtok(NULL, needle);
      index++;
    }
    // Create new parameters with masked password
    sprintf(newParam, " --username %s --password ****", username);
    memset(g.param, '\0', MAXCMD);
    strcpy(g.param, newParam);
  } else { // GIT
    // First : from http://
    index = strcspn(g.URL, ":");
    // Second after username
    secondIndex = strcspn(g.URL + index + 1, ":");
    index = index + secondIndex + 1;
    if(index < strlen(g.URL)) {  // Contains second :
      beg = (char *)malloc(index + 2);
      memset(beg, '\0', index + 2);
      strncpy(beg, g.URL, index + 1);
      // Place where password ends
      end = strchr(g.URL, '@');
      sprintf(newParam, "%s****%s", beg, end);
      strcpy(g.URL, newParam);
    }
  }
}

/**
 * \brief get the command to run to get files from version control system
 * \param int withPassword true to make command with actual or false to mask password
 * \return char* null terminated string
 */
char* GetVersionControlCommand(int withPassword)
{
  char Type[][4] = {"SVN", "Git", "CVS"};
  char *command = NULL;
  char *tmp_file_dir;
  int ret = 0;

  /** save each upload files in /srv/fossology/repository/localhost/wget/wget.xxx.dir/ */
  if (asprintf(&tmp_file_dir, "%s.dir", g.temp_file) == -1)
  {
    return NULL;
  }

  if(withPassword < 0) {
    MaskPassword();
  }

  if (strcmp(g.type, Type[0]) == 0)
  {
    if (g.proxy[0] && g.proxy[0][0])
    {
      ret = asprintf(&command, "svn --config-option servers:global:http-proxy-host=%s --config-option servers:global:http-proxy-port=%s export %s %s %s --no-auth-cache >/dev/null 2>&1", g.proxy[4], g.proxy[5], g.URL, g.param, tmp_file_dir);
    } else {
      ret = asprintf(&command, "svn export %s %s %s --no-auth-cache >/dev/null 2>&1", g.URL, g.param, tmp_file_dir);
    }

  } else if (strcmp(g.type, Type[1]) == 0)
  {
    replace_url_with_auth();
    if (g.proxy[0] && g.proxy[0][0])
    {
      ret = asprintf(&command, "git config --global http.proxy %s && git clone %s %s %s  && rm -rf %s/.git", g.proxy[0], g.URL, g.param, tmp_file_dir, tmp_file_dir);
    } else {
      ret = asprintf(&command, "git clone %s %s %s >/dev/null 2>&1 && rm -rf %s/.git", g.URL, g.param, tmp_file_dir, tmp_file_dir);
    }

  }

  free(tmp_file_dir);
  if (ret == -1) {
    return NULL;
  }

  return command;
}
