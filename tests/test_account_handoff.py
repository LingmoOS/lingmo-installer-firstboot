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
    def test_installer_and_completion_preserve_account_and_preference(self):
        installer = SOURCE.parent / "lingmo-installer/src/modules/displaymanager/main.py"
        module = ast.parse(installer.read_text())
        klass = next(n for n in module.body if isinstance(n, ast.ClassDef) and n.name == "DMlingmodm")
        method = next(n for n in klass.body if isinstance(n, ast.FunctionDef) and n.name == "set_autologin")
        for enabled in (False, True):
            with self.subTest(enabled=enabled), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);(root / "etc").mkdir()
                (root / "etc/lingmo-dm.conf").write_text("[Theme]\nName=existing\n")
                storage = SimpleNamespace(value=lambda key: "alice" if key == "username" else None)
                namespace = {"os": __import__("os"), "libcalamares": SimpleNamespace(globalstorage=storage)}
                exec(compile(ast.Module(body=[method], type_ignores=[]), str(installer), "exec"), namespace)
                namespace["set_autologin"](SimpleNamespace(root_mount_point=temp), "alice" if enabled else None, enabled, None)
                config = configparser.ConfigParser();config.read(root / "etc/lingmo-dm.conf")
                self.assertEqual(config["Autologin"]["User"], "alice")
                self.assertEqual(config["Autologin"]["Session"], "lingmo-oobe")
                finish.finish(root=root, lookup=lambda user: SimpleNamespace(pw_uid=1000))
                config.read(root / "etc/lingmo-dm.conf")
                self.assertEqual(config["Autologin"]["User"], "alice" if enabled else "")
                self.assertEqual(config["Autologin"]["Session"], "lingmo-wayland")
                self.assertEqual(config["Theme"]["Name"], "existing")
                self.assertTrue((root / "etc/.firstboot-complete").exists())

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
