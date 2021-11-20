use rand::Rng;
use std::{
    fs::OpenOptions,
    io::{Read, Write},
    thread,
};
#[test]
fn test_multi_write_and_read() {
    let num = rand::thread_rng().gen_range(50..=100);
    for _ in 0..num {
        test_multi_write();
    }
}
fn test_multi_write() {
    let pipe_dev = OpenOptions::new()
        .write(true)
        .read(true)
        .open("/dev/pipe_dev")
        .unwrap();
    let mut handle_list = Vec::new();
    let thread_num = rand::thread_rng().gen_range(10..20);
    println!("thread_num: {}", thread_num);
    for _ in 0..thread_num {
        let buf = "12345";
        let mut pipe = pipe_dev.try_clone().unwrap();
        let handle = thread::spawn(move || {
            pipe.write(buf.as_bytes()).unwrap();
            pipe.flush().unwrap();
        });
        handle_list.push(handle);
    }
    for handle in handle_list {
        handle.join().unwrap();
    }
    check_file_content(thread_num);
}

fn check_file_content(thread_num: usize) {
    let mut pipe_dev = OpenOptions::new()
        .write(true)
        .read(true)
        .open("/dev/pipe_dev")
        .unwrap();
    let mut buf = Vec::new();
    pipe_dev.read_to_end(&mut buf).unwrap();

    for i in 0..thread_num {
        assert_eq!(&buf[i * 5..(i + 1) * 5], "12345".as_bytes());
    }
}
