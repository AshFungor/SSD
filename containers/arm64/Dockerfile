FROM dockcross/linux-arm64

RUN apt update -y && apt upgrade -y
RUN apt install -y python3

RUN pip install --break-system-packages conan

WORKDIR /project

COPY . /project

RUN conan profile detect
# RUN mv ./data/arm64.txt /root/.conan2/profiles/default
RUN conan install . --output-folder=./temp/ --build=missing

CMD ./scripts/build.sh -c && ./scripts/build.sh -b