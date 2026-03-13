Five pirates looted a chest full of 100 gold coins. Being a bunch of democratic pirates,
they agree on the following method to divide the loot:

The most senior pirate will propose a distribution of the coins. All pirates, including the
most senior pirate, will then vote. If at least 50% of the pirates (3 pirates in this case)
accept the proposal, the gold is divided as proposed. If not, the most senior pirate will be
fed to shark and the process starts over with the next most senior pirate ... The process is
repeated until a plan is approved. You can assume that all pirates are perfectly rational:
they want to stay alive first and to get as much gold as possible second. Finally, being
blood-thirsty pirates, they want to have fewer pirates on the boat if given a choice
between otherwise equal outcomes.

How will the gold coins be divided in the end?

Solution:(dynamic programing)

start from one jounior pirate 1, one senior 2:
- 2 make the decision, all goes to 2, because it's already 50%. (0 , 100)

add a more senior 3:
- 3: if 3's plan is vooted down: (0, 100, shark), so 3 can buy 1 's vote by 1 coin (1, 0, 99)

add a more senoir 4
- 4: if 4's plan is voted down: (1, 0, 99, shark), so 4 can buy 2's vote by 1 coin (0, 1, 0, 99)
add a more senior 5
- 5: if 5's plan is voted down: (0, 1,0,99, shark), so 5 can buy 1, 3 's votes by 1  coin:(1,0,1,0,98)

so will be 1,0,1,0,98.
