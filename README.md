# CSpotter
Nästet representing

Skapa ett nyckelpar på er dator och skicka den publika nyckeln till
nästetdatorn. Nyckeln ska sparas i ~/.ssh/authorized_keys, men skriv
inte över existerande nycklar. Lägg istället nyckeln i slutet på
filen.

    cat my_id_rsa.pub >> ~/.ssh/authorized_keys

Om ni vill kan ni använda shell-skriptet key-gen.sh för att generera
nycklar.

Anslut därefter till nästetdatorn med

    ssh nastet@ip

där ip-addressen fås från kommandot

    /sbin/ifconfig

på nästetdatorn. Den ip-addressen man ska använda står efter "inet
addr" under rubriken wlan0.
