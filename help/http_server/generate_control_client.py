"""Generate the full-duplex Medialon TCP client for C-Play's REST action catalog."""
import argparse
import configparser
import json
from pathlib import Path

from generate_control_events import event_variables, load_model

ROOT = Path(__file__).resolve().parents[2]


def generate(model_path, output, address="127.0.0.1", port=7008, events_model_path=None):
    if not 1 <= port <= 65535 or not address.strip() or any(c in address for c in "\r\n"):
        raise ValueError("Expected an address and port 1-65535")
    model = json.loads(Path(model_path).read_text(encoding="utf-8"))
    if model.get("version") != 1 or not isinstance(model.get("commands"), list):
        raise ValueError("Expected version-1 action catalog")
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read(Path(__file__).with_name("Showmaster_TCP_Server.mllc"), encoding="ascii")
    description = "Sends C-Play REST actions over TCP. String parameters must be URL percent encoded."
    if events_model_path:
        description += " Receives subscribed C-Play values over CR-terminated frames."
    config["INFOS"].update(Title="C-Play TCP Control Client", Version="1.1.0" if events_model_path else "1.0.0",
                           Description=description)
    config["TYPE"] = {"Communication": "TCP/IP Client", "Address": address,
                      "Port": str(port), "Auto reconnection": "1", "Optimize packets": "0"}
    config["FRAMES"]["Keep frames"] = "0"
    # The parameter syntax is the same as Medialon's Winamp TCP client:
    # [Parameter name,string,literal,0]. Escape '&' in static query separators.
    commands = {"Count": str(len(model["commands"]))}
    paths = set()
    for i, action in enumerate(model["commands"]):
        path = action["path"]
        params = action["parameters"]
        if path in paths or not path.startswith("/") or not path.isascii():
            raise ValueError("Invalid or duplicate action path")
        paths.add(path)
        query = "!26".join(f"{name}=[{name},string,literal,0]" for name in params)
        frame = "POST " + path + ("?" + query if query else "") + "!0D!0A"
        commands[f"Command{i}.Name"] = action["name"]
        commands[f"Command{i}.Frame"] = frame
    config["COMMANDS"] = commands
    monitoring = {"FixedLengthChar": "X", "VariableLengthChar": "?",
                  "Requests count": "0", "Wait for answer": "0",
                  "Timeout Answer": "3000", "Variables count": "0"}
    if events_model_path:
        # Full duplex: C-Play sends subscribed values to this client with the EventModel
        # frames when its incoming client profile uses that structure with a CR terminator.
        # Frames end at CR so LF row separators in formatted lists stay inside the value.
        config["FRAMES"]["Ending character"] = "!0D"
        config["FRAMES"]["Keep frames"] = "1"
        events = load_model(events_model_path)
        reply = dict(Name="Last_Reply", Frame="?!0D", Position=0, Length=1, Type="string", Format="literal",
                     UpdateMode=0, EnumNameCount=0, BEnumIntegerValues=0)
        monitoring.update({f"Variable0.{key}": str(value) for key, value in reply.items()})
        monitoring.update(event_variables(events, 1))
        monitoring["Variables count"] = str(1 + len(events["commands"]))
    config["MONITORING"] = monitoring
    config["ANSWERS"] = {"For each mode": "0", "For each frame": "", "Count": "0"}
    output = Path(output)
    with output.open("w", encoding="ascii", newline="\r\n") as file:
        config.write(file, space_around_delimiters=False)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, default=ROOT / "data/TcpControlActions.json")
    parser.add_argument("--events-model", type=Path, default=ROOT / "data/EventModel.json",
                        help="EventModel defining the received monitoring variables")
    parser.add_argument("--no-events", action="store_true",
                        help="Generate a send-only client with LF framing and no monitoring variables")
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("CPlay_Control_Client.mllc"))
    parser.add_argument("--address", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7008)
    args = parser.parse_args()
    events = None if args.no_events else args.events_model
    print(f"Generated {generate(args.model, args.output, args.address, args.port, events).resolve()}")
