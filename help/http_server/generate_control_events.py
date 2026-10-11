"""Generate the Medialon LLC event receiver from C-Play's shared EventModel."""
import argparse
import configparser
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def generate(model_path, output, port=9000):
    if not 1 <= port <= 65535:
        raise ValueError("port must be between 1 and 65535")
    model = json.loads(Path(model_path).read_text(encoding="utf-8"))
    if model.get("version") != 1 or not isinstance(model.get("commands"), list):
        raise ValueError("Expected version-1 EventModel commands")
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    # Checked-in base driver keeps generation independent of Medialon installation.
    config.read(Path(__file__).with_name("Showmaster_TCP_Server.mllc"), encoding="ascii")
    config["INFOS"]["Title"] = "C-Play Control Events"
    config["INFOS"]["Description"] = "Receives C-Play scalar values and formatted lists over CRLF TCP."
    config["TYPE"]["Port"] = str(port)
    monitoring = config["MONITORING"]
    variables = dict(monitoring)
    offset = int(monitoring["Variables count"])
    variables["Variables count"] = str(offset + len(model["commands"]))
    names, prefixes = set(), set()
    for index, command in enumerate(model["commands"]):
        mode = command["eventPayload"]
        prefix = command["payload"]
        name = command["monitoringVariable"]
        variable_type = command["monitoringType"]
        width = command.get("charsPerItem", 0)
        if (mode != "value" or variable_type not in ("integer", "string")
                or type(width) is not int or not 0 <= width <= 4096
                or command.get("encoding") != "text" or command.get("raw")
                or len(command.get("eventTopics", [])) != 1
                or name in names or prefix in prefixes
                or not prefix.isascii() or any(c in prefix for c in "!?X\r\n")):
            raise ValueError(f"Invalid or duplicate event command: {name}")
        names.add(name)
        prefixes.add(prefix)
        fields = dict(Name=name, Frame=prefix + "?!0D!0A", Position=len(prefix), Length=1,
                      Type=variable_type, Format="literal",
                      UpdateMode=0, EnumNameCount=0, BEnumIntegerValues=0)
        for key, value in fields.items():
            variables[f"Variable{offset + index}.{key}"] = str(value)
    config["MONITORING"] = variables
    output = Path(output)
    with output.open("w", encoding="ascii", newline="\r\n") as file:
        config.write(file, space_around_delimiters=False)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, default=ROOT / "data" / "EventModel.json")
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("CPlay_Control_Events.mllc"))
    parser.add_argument("--port", type=int, default=9000)
    args = parser.parse_args()
    print(f"Generated {generate(args.model, args.output, args.port).resolve()}")
