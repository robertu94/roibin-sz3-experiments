from fedora:35 as builder

RUN dnf update -y && \
    dnf install -y gcc-g++ gfortran glib-devel libtool findutils file pkg-config lbzip2 git tar zip patch xz python3-devel coreutils m4 automake autoconf cmake openssl-devel openssh-server openssh bison bison-devel gawk && \
    dnf clean all -y && \
    groupadd demo && \
    useradd demo -d /home/demo -g demo
RUN echo "demo    ALL=(ALL)       NOPASSWD: ALL" >> /etc/sudoers.d/demo
COPY container_startup.sh /etc/profile.d/container_startup.sh
COPY --chown=demo:demo CMakeLists.txt  run_all.sh spack.yaml /app
COPY --chown=demo:demo src /app/src
COPY --chown=demo:demo include /app/include
COPY --chown=demo:demo share /app/share
COPY --chown=demo:demo robertu94_packages /app/robertu94_packages
RUN ln -s /usr/lib64/libpthread.so.0 /usr/lib64/libpthread.so
RUN su demo -c "git clone --depth=1 https://github.com/spack/spack /app/spack"
WORKDIR /app
USER demo
RUN source /etc/profile &&\
    spack compiler find &&  \
    spack external find && \
    spack repo add /app/robertu94_packages && \
    spack install && \
    spack gc -y && \
    spack clean -a
RUN source ./spack/share/spack/setup-env.sh &&\
    spack env activate . && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc)
RUN find -L /app/.spack-env/view/* -type f -exec readlink -f '{}' \; | \
    grep -v 'nsight-compute' | \
    xargs file -i | \
    grep 'charset=binary' | \
    grep 'charset=binary' | \
    grep 'x-executable\|x-archive\|x-sharedlib' | \
    awk -F: '{print $1}' | xargs strip -s


from fedora:35 as final
RUN dnf update -y && \
    dnf install -y libgfortran python3-devel libstdc++ openssh-clients && \
    dnf clean all -y && \
    groupadd demo && \
    useradd demo -d /home/demo -g demo && \
    ln -s /usr/lib64/libpthread.so.0 /usr/lib64/libpthread.so
RUN echo "demo    ALL=(ALL)       NOPASSWD: ALL" >> /etc/sudoers.d/demo
COPY container_startup.sh /etc/profile.d/container_startup.sh
COPY --from=builder --chown=demo:demo /app /app
WORKDIR /app
USER demo
