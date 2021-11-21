use rand::Rng;
use std::{
    fs::OpenOptions,
    io::{Read, Write},
    thread,
};
#[test]
fn test_multi_write_and_read() {
    let num = rand::thread_rng().gen_range(50..=100);
    let mut handle_list = Vec::new();
    for _ in 0..num {
        let handle = thread::spawn(move || {
            let mut pipe_dev = OpenOptions::new()
                .read(true)
                .write(true)
                .open("/dev/pipe_dev")
                .unwrap();

            pipe_dev.write(b"12345").unwrap();
        });
        handle_list.push(handle);
    }

    for handle in handle_list {
        handle.join().unwrap();
    }

    check_file_content(num);
}
#[test]
fn test_multi_empty_read() {
    let num = rand::thread_rng().gen_range(50..=100);

    for _ in 0..num {
        thread::spawn(|| {
            let mut pipe_dev = OpenOptions::new()
                .read(true)
                .write(true)
                .open("/dev/pipe_dev")
                .unwrap();

            let mut buf = Vec::new();

            pipe_dev.read_to_end(&mut buf).unwrap();

            assert_eq!(buf.len(), 0);
        });
    }
}
#[test]
fn test_multi_empty_write() {
    let num = rand::thread_rng().gen_range(50..=100);

    for _ in 0..num {
        thread::spawn(|| {
            let mut pipe_dev = OpenOptions::new()
                .read(true)
                .write(true)
                .open("/dev/pipe_dev")
                .unwrap();

            let mut buf = Vec::new();

            pipe_dev.write_all(&mut buf).unwrap();
        });
    }
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
