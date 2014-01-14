import os, os.path, shutil
import string

sievePathBase = '/var/vmail/sieve'

# TODO unset OOF
# start and end date should be in the form YYYY-MM-DD trailing zeroes must be included
def setOOF(vdomain, user, start, end, subject, message):
    path = _sievePath(vdomain, user)
    if os.path.isfile(path):
        bak = path + '.old'
        shutil.copyfile(path, bak)
        shutil.copystat(path, bak)
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

    script = _scriptForOOF(start, end, subject, message)
    f = open(path, 'w')
    f.write(script)
    f.close()
    os.chmod(path, 0600)

# TODO: check if message has  " or '
def _scriptForOOF(start, end, subject, message):
    scriptTemplate = string.Template("""
require ["date","relational","vacation"];

if allof(currentdate :value "ge" "date" "$start",
         currentdate :value "le" "date" "$end")
{
   vacation  :days 7 :subject "$subject" "$message" ;
}
    """)

    return scriptTemplate.substitute(start=start, end=end, subject=subject, message=message)

def _sievePath(vdomain, user):
    return sievePathBase + '/' + vdomain + '/' + user + '/script'
