import unittest
import subprocess

class TestProgram(unittest.TestCase):

    def get_output(self, args, retcode=0):
        proc = subprocess.Popen(args, stdin=None, stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE)
        out, err = proc.communicate()
        self.assertEqual(retcode, proc.returncode,
            "Unexpected return code %d, stdout=%r, stderr=%r" % (proc.returncode, out, err))
        return out, err

class OpenChangeTestCase(TestProgram):

    progpath = "../../bin/openchangeclient"

    def verify_command_noerrors(self, command, expectedStandardOutput):
        out, err = self.get_output(command)
        self.assertEquals(err, "")
        self.assertEquals(out, expectedStandardOutput)

    def test_help(self):
        refout = open("expected_output--help.txt").read()
        self.verify_command_noerrors([self.progpath, "--help"], refout)
        self.verify_command_noerrors([self.progpath, "-?"], refout)

    def test_version(self):
        refout = open("expected_output--version.txt").read()
        self.verify_command_noerrors([self.progpath, "--version"], refout)

    def test_usage(self):
        refout = open("expected_output--usage.txt").read()
        self.verify_command_noerrors([self.progpath, "--usage"], refout)

    def test_ocpf_syntax1(self):
        refout = open("expected_output--ocpf-syntax1.txt").read()
        self.verify_command_noerrors([self.progpath, "--ocpf-syntax", "--ocpf-file=../../libocpf/examples/sample_appointment.ocpf"], refout)

    def test_ocpf_syntax2(self):
        refout = open("expected_output--ocpf-syntax2.txt").read()
        self.verify_command_noerrors([self.progpath, "--ocpf-syntax", "--ocpf-file=../../libocpf/examples/sample_task.ocpf"], refout)

if __name__ == '__main__':
     unittest.main()
