#!/bin/bash


function runGensfen {
  [ -z "${SIZE}" ]     && echo ":: SIZE option is required" && exit 1
  [ -z "${DATA_DIR}" ] && echo ":: DATA_DIR option is required" && exit 1

  SEED=$(date '+%N')
  NAME=${DATA_DIR}/gensfen-${SIZE}-${SEED}

  OPTIONS=(
    "loop ${SIZE}"
    "eval_limit ${EVAL_LIMIT:-1000}"
    "seed ${SEED} output_file_name ${NAME} sfen_format binpack"
    "depth ${DEPTH:-3}"
    "random_multi_pv ${MULTI_PV:-3}"
    "write_minply ${MIN_PLY:-10}"
    "ensure_quiet"
    "${OPTIONS}"
  )

  thirdparty/Stockfish/src/stockfish <<EOF
uci
setoption name PruneAtShallowDepth value false
setoption name Use NNUE value false
setoption name Threads value ${THREADS:-4}
setoption name Hash value ${HASH:-2048}
isready
gensfen ${OPTIONS[@]}
EOF
}


function Main {
  case "$1" in
    init)
      bash src/nn/training/script.sh hack_gitmodules
      bash src/nn/training/script.sh hack_cmake
      bash src/nn/training/script.sh install_misc
    ;;

    hack_gitmodules)
      echo ":: BEFORE"
      cat .gitmodules
      sed -i 's#git@github.com:#https://github.com/#' .gitmodules
      echo ":: AFTER"
      cat .gitmodules
      git submodule update --init
    ;;

    hack_cmake)
      apt-get remove cmake
      pip install cmake --upgrade
    ;;

    install_misc)
      apt-get install clang lld
    ;;

    build_stockfish)
      echo ":: Building Stockfish..."
      cd thirdparty/Stockfish/src
      make -j build
    ;;

    gensfen)
      runGensfen
    ;;

    build_preprocess)
      python script.py init --clang
      python script.py build --t nn_preprocess
      python script.py build --t nn_preprocess_move
    ;;

    *)
      echo ":: Command not found [command = \"$1\"]";
      exit 1
    ;;
  esac
}

Main "${@}"
