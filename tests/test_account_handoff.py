import ast
import configparser
import importlib.util
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

SOURCE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("finish_setup", SOURCE / "finish-setup.py")
finish = importlib.util.module_from_spec(spec)
spec.loader.exec_module(finish)

class AccountHandoff(unittest.TestCase):
    def test_legacy_completion_preserves_login_preference(self):
        for enabled in (False, True):
            with self.subTest(enabled=enabled), tempfile.TemporaryDirectory() as temp:
                root=Path(temp);(root/"etc").mkdir()
                (root/"etc/lingmo-dm.conf").write_text("[Theme]\nName=existing\n[Autologin]\nUser=alice\nSession=lingmo-oobe\n")
                (root/"etc/lingmo-oobe-account.conf").write_text("[Account]\nUser=alice\nAutologin={}\n".format(str(enabled).lower()))
                finish.finish(root=root,lookup=lambda user:SimpleNamespace(pw_uid=1000))
                config=configparser.ConfigParser();config.read(root/"etc/lingmo-dm.conf")
                self.assertEqual(config["Autologin"]["User"],"alice" if enabled else "")
                self.assertEqual(config["Autologin"]["Session"],"lingmo-wayland")
                self.assertEqual(config["Theme"]["Name"],"existing")
                self.assertTrue((root/"etc/.firstboot-complete").exists())

    def test_missing_account_never_marks_complete(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/"etc").mkdir()
            with self.assertRaises(RuntimeError): finish.finish(root=root)
            self.assertFalse((root/"etc/.firstboot-complete").exists())

    def test_no_user_creation_or_password_unlock_in_launcher(self):
        script=(SOURCE/"oobe.sh").read_text()
        for command in ("useradd ","passwd -u", "chage "):
            self.assertNotIn(command,script)

if __name__ == "__main__": unittest.main()
