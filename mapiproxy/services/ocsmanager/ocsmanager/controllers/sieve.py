import os, os.path, shutil
import string

sievePathBase = '/var/vmail/sieve'
bakExtension = '.old.sieve'
vacationScriptHeader = "# Zentyal vacation script\n"

# TODO dont make backup if the script is already our vacation script
# start and end date should be in the form YYYY-MM-DD trailing zeroes must be included
def setOOF(vdomain, user, start, end, subject, message):
    path = _sievePath(vdomain, user)
    include = None
    if os.path.isfile(path):
        if not _isVacationScript(path):
            bak = path + bakExtension
            if not os.path.isfile(bak):
                shutil.copyfile(path, bak)
                shutil.copystat(path, bak)
            include = os.path.basename(path) + '.old'
    elif os.path.exists(path):
        raise Exception(path + "exists and it is not a regular file")
    else:
        userSieveDir = os.path.dirname(path)
        if not os.path.isdir(userSieveDir):
            if os.path.exists(userSieveDir):
                raise Exception(userSieveDir + " exists but is not a directory");

            domainSieveDir = os.path.dirname(userSieveDir)
            if not os.path.isdir(domainSieveDir):
                if os.path.exists(domainSieveDir):
                    raise Exception(domainSieveDir + " exists but is not a directory");
                elif not os.path.isdir(sievePathBase):
                    raise Exception('Base mail sieve directory ' + sievePathBase + 'does not exists')
                os.mkdir(domainSieveDir, 0755)

            os.mkdir(userSieveDir, 0700)

    script = _scriptForOOF(start, end, subject, message, include)
    f = open(path, 'w')
    f.write(script)
    f.close()
    os.chmod(path, 0600)

def unsetOOF(vdomain, user):
    path = _sievePath(vdomain, user)
    if not os.path.isfile(path):
        # nothing to do
        return

    bak = path + bakExtension
    if os.path.isfile(bak):
        shutil.copyfile(bak, path)
        shutil.copystat(bak, path)
        os.unlink(bak)
    else:
        os.unlink(path)

def _isVacationScript(path):
    f = open(path, 'r')
    line = f.readline(200)
    return line == vacationScriptHeader


# TODO: check if message has  " or '
def _scriptForOOF(start, end, subject, message, include):
    scriptTemplate = string.Template("""$header
require ["date","relational","vacation"$extraIncludes];

if allof(currentdate :value "ge" "date" "$start",
         currentdate :value "le" "date" "$end")
{
   vacation  :days 7 :subject "$subject" "$message" ;
}
    """)

    if (include):
        extraIncludes = ',"include"'
    else:
        extraIncludes = ''

    script = scriptTemplate.substitute(
        header=vacationScriptHeader,
        extraIncludes=extraIncludes,
        start=start, end=end, subject=subject, message=message
        )
    if (include):
        script += "\n" + 'include :personal "' + include + '";'
        script += "\n"

    return script

def _sievePath(vdomain, user):
    return sievePathBase + '/' + vdomain + '/' + user + '/script'
