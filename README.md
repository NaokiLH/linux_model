# A Linux Kernel Model demo

a linux kernel model use to create a linux device which you can read or write.

### ProjectRecord
- **[项目记录]**(https://github.com/NaokiLH/linux_model/blob/master/项目记录.md)
### Build

```sh
git clone https://github.com/NaokiLH/linux_model.git
make
```

### Install

```sh
sudo insmod char_dev.ko
```

![](https://files.catbox.moe/faq2jm.png)

![](https://files.catbox.moe/4x8fga.png)

### Use

```sh
echo 114514 > /dev/char_dev
cat /dev/char_dev 
```

![](https://files.catbox.moe/g98al8.png)

