#!/bin/sh

echo "We can now configure your system!"
echo
echo "First, let's set a root password"
passwd root

echo
echo "Now let's create a regular user login"
echo
read -p "New username: " username
mkdir -p /home/$username
adduser $username

read -p "What hostname for this system? " newhostname
echo $newhostname >/etc/hostname

