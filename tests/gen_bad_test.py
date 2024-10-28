#!/bin/env python3

# a very bad test
# i use this before i commit to github to check i didn't break something (even though it might break in runtime)

import os
import stat

params: dict[str, list[str]] = {
    "RELEASE": ["0", "1"],
    "xrandr": ["0", "1"],
    "cglm": ["0", "1"],
}

file_path: str = "build_all_combinations.sh"

clean_command: str = "make clean"
back_to_normal_command: str = """make -j 4 compile RELEASE=0 xrandr=1 cglm=1
make compile_commands.json RELEASE=0 xrandr=1 cglm=1"""
compile_command: str = "make -j 4 compile"

bash_script: str = "#!/bin/env bash\n\n"


# generate all combinations of parameters
def generate_combinations(params, keys=None, current_combination=None):
    if keys is None:
        keys = list(params.keys())
    if current_combination is None:
        current_combination = {}

    if not keys:
        # add a combination to the bash script
        flags = " ".join(
            [f"{key}={value}" for key, value in current_combination.items()]
        )
        echo_command = f'echo "Building with {flags}"'
        return f"{clean_command}\n{echo_command}\n{compile_command} {flags}\n"

    key = keys[0]
    remaining_keys = keys[1:]

    result = ""
    for value in params[key]:
        current_combination[key] = value
        result += generate_combinations(params, remaining_keys, current_combination)

    return result


bash_script += generate_combinations(params)

# add final make clean and back to normal command
bash_script += f"\n{clean_command}\n"
bash_script += f"{back_to_normal_command}\n"

# save to file
with open(file_path, "w") as f:
    f.write(bash_script)


st = os.stat(file_path)
os.chmod(file_path, st.st_mode | stat.S_IEXEC)

print("Bash script generated: build_all_combinations.sh")
