"""Generate the Medialon TCP client for C-Play's REST action catalog."""
import argparse
import configparser
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def generate(model_path, output, address="127.0.0.1", port=7008):
    if not 1 <= port <= 65535 or not address.strip() or any(c in address for c in "\r\n"):
        raise ValueError("Expected an address and port 1-65535")
    model = json.loads(Path(model_path).read_text(encoding="utf-8"))
    if model.get("version") != 1 or not isinstance(model.get("commands"), list):
        raise ValueError("Expected version-1 action catalog")
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read(Path(__file__).with_name("Showmaster_TCP_Server.mllc"), encoding="ascii")
    config["INFOS"].update(Title="C-Play TCP Control Client", Version="1.0.0",
                           Description="Sends C-Play REST actions over TCP. String parameters must be URL percent encoded.")
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
    config["MONITORING"] = {"FixedLengthChar": "X", "VariableLengthChar": "?",
                            "Requests count": "0", "Wait for answer": "0",
                            "Timeout Answer": "3000", "Variables count": "0"}
    config["ANSWERS"] = {"For each mode": "0", "For each frame": "", "Count": "0"}
    output = Path(output)
    with output.open("w", encoding="ascii", newline="\r\n") as file:
        config.write(file, space_around_delimiters=False)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, default=ROOT / "data/TcpControlActions.json")
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("CPlay_Control_Client.mllc"))
    parser.add_argument("--address", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7008)
    args = parser.parse_args()
    print(f"Generated {generate(args.model, args.output, args.address, args.port).resolve()}")
