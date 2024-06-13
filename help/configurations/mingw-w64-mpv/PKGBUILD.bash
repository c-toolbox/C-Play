# Maintainer: Alexey Pavlov <alexpux@gmail.com>
# Contributor: James Ross-Gowan <rossymiles@gmail.com>

_realname=mpv
pkgbase="mingw-w64-${_realname}"
pkgname="${MINGW_PACKAGE_PREFIX}-${_realname}"
pkgver=0.38.0
pkgrel=1
pkgdesc="Video player based on MPlayer/mplayer2 (mingw-w64)"
url="https://mpv.io/"
msys2_repository_url="https://github.com/mpv-player/mpv"
msys2_references=(
  "cpe: cpe:/a:mpv:mpv"
)
arch=('any')
mingw_arch=('mingw64' 'ucrt64' 'clang64')
license=('spx:GPL-2.0-only' 'spx:LGPL-2.1-only')
depends=("${MINGW_PACKAGE_PREFIX}-ffmpeg"
         "${MINGW_PACKAGE_PREFIX}-lcms2"
         "${MINGW_PACKAGE_PREFIX}-libarchive"
         "${MINGW_PACKAGE_PREFIX}-libass"
         "${MINGW_PACKAGE_PREFIX}-libcaca"
         "${MINGW_PACKAGE_PREFIX}-libcdio"
         "${MINGW_PACKAGE_PREFIX}-libcdio-paranoia"
         "${MINGW_PACKAGE_PREFIX}-libjpeg-turbo"
         "${MINGW_PACKAGE_PREFIX}-libplacebo"
         "${MINGW_PACKAGE_PREFIX}-lua51"
         "${MINGW_PACKAGE_PREFIX}-mujs"
         "${MINGW_PACKAGE_PREFIX}-rubberband"
         "${MINGW_PACKAGE_PREFIX}-shaderc"
         "${MINGW_PACKAGE_PREFIX}-spirv-cross"
         "${MINGW_PACKAGE_PREFIX}-uchardet"
         "${MINGW_PACKAGE_PREFIX}-vapoursynth"
         "${MINGW_PACKAGE_PREFIX}-vulkan"
         "winpty")
makedepends=("${MINGW_PACKAGE_PREFIX}-cc"
             "${MINGW_PACKAGE_PREFIX}-meson"
             "${MINGW_PACKAGE_PREFIX}-ninja"
             "${MINGW_PACKAGE_PREFIX}-pkgconf"
             "${MINGW_PACKAGE_PREFIX}-python-docutils"
             "${MINGW_PACKAGE_PREFIX}-python-rst2pdf"
             "${MINGW_PACKAGE_PREFIX}-vulkan-headers")
optdepends=("${MINGW_PACKAGE_PREFIX}-yt-dlp: for video-sharing websites playback")
source=("${_realname}-${pkgver}.tar.gz"::"https://github.com/mpv-player/${_realname}/archive/v${pkgver}.tar.gz")
sha256sums=('86d9ef40b6058732f67b46d0bbda24a074fae860b3eaae05bab3145041303066')

prepare() {
  cd "${srcdir}/${_realname}-${pkgver}"
}

build() {
  mkdir -p build-${MSYSTEM} && cd build-${MSYSTEM}

  MSYS2_ARG_CONV_EXCL="--prefix=" \
    meson setup \
      --prefix="${MINGW_PREFIX}" \
      --wrap-mode=nodownload \
      --buildtype=plain \
	  -Ddefault_library=shared \
      -Dlibmpv=true \
      -Djavascript=enabled \
      ../${_realname}-${pkgver}

  meson compile
}

package() {
  cd "${srcdir}/build-${MSYSTEM}"

  DESTDIR="${pkgdir}" meson install

  # Move encoding-profiles.conf to share/doc alongside the example .conf files.
  # mpv doesn't search /etc for configuration on MinGW.
  mv "${pkgdir}${MINGW_PREFIX}/etc/mpv/"*.conf "${pkgdir}${MINGW_PREFIX}/share/doc/mpv/"

  # mpv needs winpty for key bindings to work on the terminal
  mv "${pkgdir}${MINGW_PREFIX}/bin/mpv.exe" "${pkgdir}${MINGW_PREFIX}/bin/mpv_exe"
  _exename=mpv
  echo '#!/usr/bin/env bash' > "${pkgdir}${MINGW_PREFIX}/bin/${_exename}"
  echo 'export _started_from_console=yes' >> "${pkgdir}${MINGW_PREFIX}/bin/${_exename}"
  echo '/usr/bin/winpty "$( dirname ${BASH_SOURCE[0]} )/'${_exename}'.exe" "$@"' >> "${pkgdir}${MINGW_PREFIX}/bin/${_exename}"
  mv "${pkgdir}${MINGW_PREFIX}/bin/mpv_exe" "${pkgdir}${MINGW_PREFIX}/bin/mpv.exe"
}
