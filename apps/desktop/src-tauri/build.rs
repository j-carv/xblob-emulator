use std::env;
use std::path::PathBuf;

fn main() {
    tauri_build::build();

    let target = env::var("TARGET").unwrap_or_default();

    if let Ok(core_lib_dir) = env::var("XBLOB_CORE_LIB_DIR") {
        println!("cargo:rustc-link-search=native={}", core_lib_dir);
    } else {
        let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
        let root_dir = manifest_dir
            .join("../../..")
            .canonicalize()
            .expect("Failed to locate xblob root repository directory");

        println!("cargo:rerun-if-changed={}/libs", root_dir.display());
        println!(
            "cargo:rerun-if-changed={}/CMakeLists.txt",
            root_dir.display()
        );

        let mut cfg = cmake::Config::new(&root_dir);
        cfg.define("XBLOB_BUILD_TESTS", "OFF")
            .define("XBLOB_BUILD_APPS", "OFF")
            .define("XBLOB_ENABLE_SANITIZERS", "OFF");

        let dst = cfg.build();

        println!("cargo:rustc-link-search=native={}/lib", dst.display());
        println!(
            "cargo:rustc-link-search=native={}/build/libs/c_api",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/machine",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/loader",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/cpu",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/memory",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/bus",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/core",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/formats",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/io",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/gpu",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/pci",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/kernel",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/vfs",
            dst.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/libs/common",
            dst.display()
        );
    }

    println!("cargo:rustc-link-lib=static=xblob_c_api");
    println!("cargo:rustc-link-lib=static=xblob_machine");
    println!("cargo:rustc-link-lib=static=xblob_vfs");
    println!("cargo:rustc-link-lib=static=xblob_gpu");
    println!("cargo:rustc-link-lib=static=xblob_pci");
    println!("cargo:rustc-link-lib=static=xblob_kernel");
    println!("cargo:rustc-link-lib=static=xblob_loader");
    println!("cargo:rustc-link-lib=static=xblob_cpu");
    println!("cargo:rustc-link-lib=static=xblob_memory");
    println!("cargo:rustc-link-lib=static=xblob_bus");
    println!("cargo:rustc-link-lib=static=xblob_core");
    println!("cargo:rustc-link-lib=static=xblob_formats");
    println!("cargo:rustc-link-lib=static=xblob_io");
    println!("cargo:rustc-link-lib=static=xblob_common");

    if target.contains("apple") {
        println!("cargo:rustc-link-lib=dylib=c++");
    } else if target.contains("windows-msvc") {
        // MSVC runtime handles C++ linkage automatically
    } else {
        println!("cargo:rustc-link-lib=dylib=stdc++");
    }
}
