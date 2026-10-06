"""Generate a Medialon LLC TCP receiver for C-Play."""
import argparse
import configparser
from pathlib import Path

DRIVER_DIR = Path(r"C:\ProgramData\Medialon\CommonFiles\Low Level Communicator Drivers")


def generate_pure_showmaster_server(port, filename="Showmaster_TCP_Server.mllc"):
    if not 1 <= port <= 65535:
        raise ValueError("port must be between 1 and 65535")
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    template = DRIVER_DIR / "Winamp_Telnet.v1.0.0.mllc"
    if not config.read(template, encoding="cp1252"):
        raise FileNotFoundError(f"Required LLC template not found: {template}")
    config["INFOS"] = dict(Title="Task Remote Server", Author="C-Play", Version="1.0.0", DeviceMark="Medialon", Model="Showmaster", Description="Receives task_start and task_stop with a task name and CRLF. Project task dispatch logic is required.")
    config["TYPE"] = {"Communication": "TCP/IP Server", "Port": str(port), "Bind on specified": "0", "Specified address": "0.0.0.0", "Optimize packets": "0"}
    # LLC represents control bytes as !XX, whereas C-Play uses plain hex.
    # End input on LF (confirmed in Medialon); monitoring patterns below
    # still match the complete CRLF sent by C-Play, excluding CR from names.
    config["FRAMES"]["Hexa character"] = "!"
    config["FRAMES"]["Count of bytes"] = "0"
    config["FRAMES"]["Header character"] = ""
    config["FRAMES"]["Ending character"] = "!0A"
    config["FRAMES"]["Keep frames"] = "1"
    config["FRAMES"]["Input checksum verification"] = "0"
    config["FRAMES"]["Output checksum verification"] = "0"
    config["COMMANDS"] = {"Count": "0"}
    config["POSITRACK GROUPS"] = {"Count": "0"}
    config["ANSWERS"] = {"For each mode": "0", "For each frame": "", "Count": "0"}
    config["MONITORING"] = {"FixedLengthChar": "X", "VariableLengthChar": "?", "Requests count": "0", "Wait for answer": "0", "Timeout Answer": "3000", "Variables count": "3"}
    variables = [("Last_Command", "?!0D!0A", 0), ("Start_Task_Name", "task_start ?!0D!0A", 11), ("Stop_Task_Name", "task_stop ?!0D!0A", 10)]
    for i, (name, frame, position) in enumerate(variables):
        # LLC UpdateMode=0 updates immediately upon reception; 1 waits for Get frame.
        for key, value in (("Name", name), ("Frame", frame), ("Position", position), ("Length", 1), ("Type", "string"), ("Format", "literal"), ("UpdateMode", 0), ("EnumNameCount", 0), ("BEnumIntegerValues", 0)):
            config["MONITORING"][f"Variable{i}.{key}"] = str(value)
    output = Path(filename)
    with output.open("w", encoding="ascii", newline="\r\n") as file:
        config.write(file, space_around_delimiters=False)
    print(f"Generated {output.resolve()}")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("Showmaster_TCP_Server.mllc"))
    args = parser.parse_args()
    generate_pure_showmaster_server(args.port, args.output)
