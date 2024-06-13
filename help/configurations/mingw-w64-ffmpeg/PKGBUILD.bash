# Maintainer: Alexey Pavlov <Alexpux@gmail.com>
# Contributor: Zach Bacon <11doctorwhocanada@gmail.com>
# Contributor: wirx6 <wirx654@gmail.com>
# Contributor: Ray Donnelly <mingw.android@gmail.com>

_realname=ffmpeg
pkgbase="mingw-w64-${_realname}"
pkgname="${MINGW_PACKAGE_PREFIX}-${_realname}"
pkgver=6.1.1
pkgrel=10
pkgdesc="Complete solution to record, convert and stream audio and video (mingw-w64)"
arch=('any')
mingw_arch=('mingw32' 'mingw64' 'ucrt64' 'clang64' 'clang32' 'clangarm64')
url="https://ffmpeg.org/"
msys2_repository_url='https://git.ffmpeg.org/ffmpeg.git'
msys2_references=(
  "cpe: cpe:/a:ffmpeg:ffmpeg"
)
license=('spdx:GPL-3.0-or-later')
depends=("${MINGW_PACKAGE_PREFIX}-aom"
         "${MINGW_PACKAGE_PREFIX}-bzip2"
         "${MINGW_PACKAGE_PREFIX}-frei0r-plugins"
         "${MINGW_PACKAGE_PREFIX}-dav1d"
         "${MINGW_PACKAGE_PREFIX}-gmp"
         "${MINGW_PACKAGE_PREFIX}-gsm"
         "${MINGW_PACKAGE_PREFIX}-lame"
         "${MINGW_PACKAGE_PREFIX}-libass"
         "${MINGW_PACKAGE_PREFIX}-libexif"
         "${MINGW_PACKAGE_PREFIX}-libiconv"
         "${MINGW_PACKAGE_PREFIX}-libjxl"
         "${MINGW_PACKAGE_PREFIX}-libplacebo"
         "${MINGW_PACKAGE_PREFIX}-libtheora"
         "${MINGW_PACKAGE_PREFIX}-libva"
         "${MINGW_PACKAGE_PREFIX}-libvorbis"
         "${MINGW_PACKAGE_PREFIX}-libvpx"
         "${MINGW_PACKAGE_PREFIX}-libwebp"
         "${MINGW_PACKAGE_PREFIX}-libvpl"
         "${MINGW_PACKAGE_PREFIX}-openal"
         "${MINGW_PACKAGE_PREFIX}-opencore-amr"
         "${MINGW_PACKAGE_PREFIX}-openjpeg2"
         "${MINGW_PACKAGE_PREFIX}-opus"
		 "${MINGW_PACKAGE_PREFIX}-jack2"
         "${MINGW_PACKAGE_PREFIX}-rav1e"
         "${MINGW_PACKAGE_PREFIX}-rtmpdump"
          $([[ "${CARCH}" == "i686" ]] || echo "${MINGW_PACKAGE_PREFIX}-svt-av1")
         "${MINGW_PACKAGE_PREFIX}-vulkan"
         "${MINGW_PACKAGE_PREFIX}-libx264"
         "${MINGW_PACKAGE_PREFIX}-x265"
         "${MINGW_PACKAGE_PREFIX}-xvidcore"
         "${MINGW_PACKAGE_PREFIX}-zimg"
         "${MINGW_PACKAGE_PREFIX}-zlib")
makedepends=("${MINGW_PACKAGE_PREFIX}-cc"
             "${MINGW_PACKAGE_PREFIX}-autotools"
             "${MINGW_PACKAGE_PREFIX}-pkgconf"
             "${MINGW_PACKAGE_PREFIX}-dlfcn"
             "${MINGW_PACKAGE_PREFIX}-vulkan-headers"
             $([[ ${MINGW_PACKAGE_PREFIX} == *-clang-aarch64* ]] || echo "${MINGW_PACKAGE_PREFIX}-amf-headers")
             $([[ ${MINGW_PACKAGE_PREFIX} == *-clang-aarch64* ]] || echo "${MINGW_PACKAGE_PREFIX}-ffnvcodec-headers")
             $([[ ${MINGW_PACKAGE_PREFIX} == *-clang-aarch64* ]] || echo "${MINGW_PACKAGE_PREFIX}-nasm"))
source=(https://ffmpeg.org/releases/${_realname}-${pkgver}.tar.xz{,.asc}
        "pathtools.c"
        "pathtools.h"
        "0001-rename-the-bundled-Mesa-AV1-vulkan-video-headers.patch::https://git.ffmpeg.org/gitweb/ffmpeg.git/patch/fef22c87"
        "0005-Win32-Add-path-relocation-to-frei0r-plugins-search.patch"
        "0009-wrong-null-type.patch"
        "https://github.com/FFmpeg/FFmpeg/commit/06c2a2c425f22e7dba5cad909737a631cc676e3f.patch"
        "https://github.com/FFmpeg/FFmpeg/commit/43b417d516b0fabbec1f02120d948f636b8a018e.patch")
validpgpkeys=('FCF986EA15E6E293A5644F10B4322F04D67658D8')
sha256sums=('8684f4b00f94b85461884c3719382f1261f0d9eb3d59640a1f4ac0873616f968'
            'SKIP'
            'ebf471173f5ee9c4416c10a78760cea8afaf1a4a6e653977321e8547ce7bf3c0'
            '1585ef1b61cf53a2ca27049c11d49e0834683dfda798f03547761375df482a90'
            'c2ef9c35082ed2e5989428d086b7bfef1dfe9e0a85e6d259daf46f369f115483'
            '8c74e9b5800dbb41c33a60114712726ec768ad6de8f147c2eb30656fd4c899cc'
            'f76048e6e1944e15f646a52b75e75bc8906ca80f2a3124fba6a2050722750447'
            'd72cf07b112d50e346df490fdfc374d3746e8224ad139858d559bd04bc7def51'
            'bb144ba747f494cbb386f0032d45df5657a8017b5812f4868f78f263ddb98ac9')

# Helper macros to help make tasks easier #
apply_patch_with_msg() {
  for _fname in "$@"
  do
    msg2 "Applying ${_fname}"
    patch -Nbp1 -i "${srcdir}"/${_fname}
  done
}

prepare() {
  test ! -d "${startdir}/../mingw-w64-pathtools" || {
    cmp "${startdir}/../mingw-w64-pathtools/pathtools.c" "${srcdir}/pathtools.c" &&
    cmp "${startdir}/../mingw-w64-pathtools/pathtools.h" "${srcdir}/pathtools.h"
  } || exit 1

  cd "${srcdir}/${_realname}-${pkgver}"
  cp -fHv "${srcdir}"/pathtools.[ch] libavfilter/

  apply_patch_with_msg \
    0001-rename-the-bundled-Mesa-AV1-vulkan-video-headers.patch \
    0005-Win32-Add-path-relocation-to-frei0r-plugins-search.patch

  apply_patch_with_msg 0009-wrong-null-type.patch

  apply_patch_with_msg \
    06c2a2c425f22e7dba5cad909737a631cc676e3f.patch \
    43b417d516b0fabbec1f02120d948f636b8a018e.patch
}

build() {
  local -a common_config
  common_config+=(
    --disable-debug
    --disable-stripping
    --disable-doc
	--disable-sdl2
    --enable-dxva2
    --enable-d3d11va
    --enable-frei0r
    --enable-gmp
    --enable-gpl
    --enable-iconv
    --enable-libaom
    --enable-libass
    --enable-libdav1d
    --enable-libgsm
    --enable-libjxl
    --enable-libmp3lame
    --enable-libopencore_amrnb
    --enable-libopencore_amrwb
    --enable-libopenjpeg
    --enable-libopus
    --enable-libplacebo
    --enable-librtmp
    --enable-libtheora
    --enable-libvorbis
    --enable-libx264
    --enable-libx265
    --enable-libxvid
    --enable-libvpx
    --enable-libwebp
	--enable-libjack
    --enable-libzimg
    --enable-openal
	--enable-opengl
    --enable-pic
    --enable-postproc
    --enable-runtime-cpudetect
    --enable-swresample
    --enable-version3
    --enable-vulkan
    --enable-zlib
    --enable-librav1e
    --enable-libvpl
  )

  if [[ "${CARCH}" != "i686" ]]; then
    common_config+=(
       --enable-libsvtav1
    )
  fi

  if [[ "${MINGW_PACKAGE_PREFIX}" != *clang-aarch64* ]]; then
    common_config+=(
        --enable-amf
        --enable-nvenc
		--enable-nvdec
    )
  fi

  # https://github.com/msys2/MINGW-packages/pull/20930#issuecomment-2117626198
  CFLAGS+=" -Wno-error=incompatible-pointer-types"

  for _variant in -static -shared; do
    [[ -d "${srcdir}/build-${MSYSTEM}${_variant}" ]] && rm -rf "${srcdir}/build-${MSYSTEM}${_variant}"
    mkdir -p "${srcdir}/build-${MSYSTEM}${_variant}" && cd "${srcdir}/build-${MSYSTEM}${_variant}"
    if [[ ${_variant} == -static ]]; then
      ENABLE_VARIANT="--enable-static --pkg-config-flags=--static"
    else
      ENABLE_VARIANT=--enable-shared
    fi
    ../${_realname}-${pkgver}/configure \
      --prefix=${MINGW_PREFIX} \
      --target-os=mingw32 \
      --arch=${CARCH%%-*} \
      --cc=${CC} \
      --cxx=${CXX} \
      "${common_config[@]}" \
      --logfile=config.log \
      ${ENABLE_VARIANT}

    make
  done
}

check() {
  for _variant in -static -shared; do
    cd "${srcdir}/build-${MSYSTEM}${_variant}"
    # workaround for conflict with SDL main(), use it if you have SDL installed
    # make check CC_C="-c -Umain"
    make check || true
  done
}

package() {
  for _variant in -static -shared; do
    cd "${srcdir}/build-${MSYSTEM}${_variant}"
    make DESTDIR="${pkgdir}" install
  done
  
  rm -f ${pkgdir}/${MINGW_PREFIX}/lib/*.def
  rm -f ${pkgdir}/${MINGW_PREFIX}/bin/*.lib

  local PREFIX_DEPS=$(cygpath -am ${MINGW_PREFIX})
  find ${pkgdir}${MINGW_PREFIX}/lib/pkgconfig -name *.pc -exec sed -i -e"s|${PREFIX_DEPS}|${MINGW_PREFIX}|g" {} \;
}
